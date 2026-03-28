#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="${PYTHON_BIN:-python3}"
VENV_DIR="${ROOT_DIR}/.venv"
SERVER_URL="${SERVER_URL:-}"
NODE_ID="${NODE_ID:-laptop-vision-node}"
CAMERA_INDEX="${CAMERA_INDEX:-0}"
FPS="${FPS:-12}"
SEND_INTERVAL_MS="${SEND_INTERVAL_MS:-120}"
MAX_SNAPSHOTS_PER_SECOND="${MAX_SNAPSHOTS_PER_SECOND:-6}"
YOLO_EVERY_N_FRAMES="${YOLO_EVERY_N_FRAMES:-4}"
MODEL_NAME="${MODEL_NAME:-yolov8n.pt}"
OBJECTS_MIN_CONFIDENCE="${OBJECTS_MIN_CONFIDENCE:-0.60}"
GESTURES_MIN_CONFIDENCE="${GESTURES_MIN_CONFIDENCE:-0.70}"
DELTA_THRESHOLD="${DELTA_THRESHOLD:-0.12}"
PIP_INDEX_URL="${PIP_INDEX_URL:-}"
TORCH_INDEX_URL="${TORCH_INDEX_URL:-https://download.pytorch.org/whl/cpu}"
DEBUG_UI=0
SKIP_INSTALL=0
NON_INTERACTIVE=0
ENV_CHECK_MESSAGE=""

log_info() {
  echo "[INFO] $*"
}

log_warn() {
  echo "[WARN] $*"
}

log_error() {
  echo "[ERROR] $*" >&2
}

sanitize_value() {
  local value="${1-}"
  value="${value//$'\r'/}"
  printf '%s' "${value}"
}

need_cmd() {
  command -v "$1" >/dev/null 2>&1
}

usage() {
  cat <<'EOF'
Usage:
  ./run_vision_node.sh [options]

Options:
  --server-url URL                 WebSocket URL exposed by the main PC
  --node-id ID                     Stable node id (default: laptop-vision-node)
  --camera-index N                 OpenCV camera index (default: 0)
  --fps N                          Target FPS (default: 12)
  --send-interval-ms N             Minimum send interval (default: 120)
  --max-snapshots-per-second N     Hard send cap (default: 6)
  --yolo-every-n-frames N          Run YOLO every N frames (default: 4)
  --model-name NAME                YOLO model name/path (default: yolov8n.pt)
  --objects-min-confidence N       Object threshold (default: 0.60)
  --gestures-min-confidence N      Gesture threshold (default: 0.70)
  --delta-threshold N              Resend delta threshold (default: 0.12)
  --pip-index-url URL              Override pip package index for dependency install
  --torch-index-url URL            PyTorch wheel index (default: CPU-only PyTorch wheels)
  --debug-ui                       Enable OpenCV debug window
  --python PATH                    Python interpreter to use
  --skip-install                   Do not offer dependency install
  --non-interactive                Fail instead of prompting for missing values
  -h, --help                       Show this help
EOF
}

prompt_value() {
  local prompt_text="$1"
  local default_value="${2:-}"
  local result=""

  if [[ "${NON_INTERACTIVE}" -eq 1 ]]; then
    echo "${default_value}"
    return
  fi

  if [[ -n "${default_value}" ]]; then
    read -r -p "${prompt_text} [${default_value}]: " result
    sanitize_value "${result:-${default_value}}"
  else
    read -r -p "${prompt_text}: " result
    sanitize_value "${result}"
  fi
}

prompt_yes_no() {
  local prompt_text="$1"
  local default_value="${2:-y}"
  local answer=""

  if [[ "${NON_INTERACTIVE}" -eq 1 ]]; then
    [[ "${default_value}" =~ ^[Yy]$ ]] && return 0 || return 1
  fi

  read -r -p "${prompt_text} [${default_value}]: " answer
  answer="${answer:-${default_value}}"
  [[ "${answer}" =~ ^[Yy]$ ]]
}

ensure_python() {
  if need_cmd "${PYTHON_BIN}"; then
    return
  fi

  if [[ "${NON_INTERACTIVE}" -eq 1 ]]; then
    log_error "Python interpreter not found: ${PYTHON_BIN}"
    exit 1
  fi

  log_warn "Python interpreter not found: ${PYTHON_BIN}"
  PYTHON_BIN="$(prompt_value "Enter a valid Python executable" "python3")"
  if ! need_cmd "${PYTHON_BIN}"; then
    log_error "Python interpreter not found: ${PYTHON_BIN}"
    exit 1
  fi
}

ensure_virtualenv() {
  if [[ -x "${VENV_DIR}/bin/python" ]]; then
    return
  fi

  if [[ "${SKIP_INSTALL}" -eq 1 ]]; then
    log_warn "Virtual environment not found at ${VENV_DIR}; using system Python."
    return
  fi

  if prompt_yes_no "Create a local virtual environment at ${VENV_DIR}?" "y"; then
    "${PYTHON_BIN}" -m venv "${VENV_DIR}"
  fi
}

check_python_environment() {
  local active_python="$1"
  local output=""
  local status=0

  set +e
  output="$("${active_python}" - <<'EOF'
import importlib.util
import sys

modules = ("cv2", "mediapipe", "ultralytics", "websockets", "torch", "torchvision")
missing = [name for name in modules if importlib.util.find_spec(name) is None]
if missing:
    print("missing:" + ",".join(missing))
    raise SystemExit(1)

import torch
cuda_version = getattr(getattr(torch, "version", None), "cuda", None)
if cuda_version:
    print(f"torch_cuda:{cuda_version}")
    raise SystemExit(2)

print("ok")
EOF
)"
  status=$?
  set -e

  ENV_CHECK_MESSAGE="$(sanitize_value "${output}")"
  return ${status}
}

install_torch_cpu_stack() {
  local active_python="$1"
  local install_attempt=1
  local max_attempts=3
  local -a install_cmd=(
    "${active_python}"
    -m pip install
    --no-cache-dir
    --prefer-binary
    --upgrade
    --force-reinstall
    --retries 5
    --timeout 60
    --index-url "${TORCH_INDEX_URL}"
    torch
    torchvision
  )

  while [[ ${install_attempt} -le ${max_attempts} ]]; do
    log_info "Installing CPU-only PyTorch stack (attempt ${install_attempt}/${max_attempts})..."
    log_info "Using PyTorch index: ${TORCH_INDEX_URL}"

    if env PIP_DISABLE_PIP_VERSION_CHECK=1 "${install_cmd[@]}"; then
      return 0
    fi

    log_warn "CPU-only PyTorch install attempt ${install_attempt} failed."
    install_attempt=$((install_attempt + 1))
    if [[ ${install_attempt} -le ${max_attempts} ]]; then
      sleep 2
    fi
  done

  return 1
}

install_requirements() {
  local active_python="$1"
  local install_attempt=1
  local max_attempts=3
  local attempt_index_url="${PIP_INDEX_URL}"
  local -a pip_args=(
    -m pip install
    --no-cache-dir
    --prefer-binary
    --retries 5
    --timeout 60
    -r "${ROOT_DIR}/requirements.txt"
  )
  local -a install_cmd=()

  while [[ ${install_attempt} -le ${max_attempts} ]]; do
    if [[ -z "${attempt_index_url}" && ${install_attempt} -ge 2 ]]; then
      attempt_index_url="https://pypi.org/simple"
    fi

    log_info "Installing Python dependencies (attempt ${install_attempt}/${max_attempts})..."
    if [[ -n "${attempt_index_url}" ]]; then
      log_info "Using pip index: ${attempt_index_url}"
    fi

    install_cmd=("${active_python}" "${pip_args[@]}")
    if [[ -n "${attempt_index_url}" ]]; then
      install_cmd+=(--index-url "${attempt_index_url}")
    fi

    if env PIP_DISABLE_PIP_VERSION_CHECK=1 "${install_cmd[@]}"; then
      return 0
    fi

    log_warn "Dependency installation attempt ${install_attempt} failed."
    install_attempt=$((install_attempt + 1))
    if [[ ${install_attempt} -le ${max_attempts} ]]; then
      sleep 2
    fi
  done

  return 1
}

ensure_requirements() {
  local active_python="${PYTHON_BIN}"
  local env_check_status=0
  if [[ -x "${VENV_DIR}/bin/python" ]]; then
    active_python="${VENV_DIR}/bin/python"
  fi

  if check_python_environment "${active_python}"; then
    PYTHON_BIN="${active_python}"
    return
  else
    env_check_status=$?
  fi

  if [[ "${SKIP_INSTALL}" -eq 1 ]]; then
    if [[ ${env_check_status} -eq 2 ]]; then
      log_warn "Current Python environment uses a CUDA-enabled torch build (${ENV_CHECK_MESSAGE}). --skip-install left it unchanged."
    else
      log_warn "Python dependencies appear to be missing and --skip-install was set."
    fi
    PYTHON_BIN="${active_python}"
    return
  fi

  if [[ ${env_check_status} -eq 2 ]]; then
    log_warn "Detected CUDA-enabled torch in the vision node environment (${ENV_CHECK_MESSAGE}). Replacing it with a CPU-only build."
  elif [[ -n "${ENV_CHECK_MESSAGE}" && "${ENV_CHECK_MESSAGE}" != "ok" ]]; then
    log_info "Python environment needs setup: ${ENV_CHECK_MESSAGE}"
  fi

  if prompt_yes_no "Install vision node Python dependencies from requirements.txt?" "y"; then
    if ! install_torch_cpu_stack "${active_python}"; then
      log_error "Unable to install the CPU-only PyTorch stack."
      log_error "You can retry later with:"
      log_error "  ${active_python} -m pip install --no-cache-dir --prefer-binary --upgrade --force-reinstall --index-url ${TORCH_INDEX_URL} torch torchvision"
      exit 1
    fi

    if ! install_requirements "${active_python}"; then
      if [[ "${NON_INTERACTIVE}" -eq 0 ]] && prompt_yes_no "Dependency install failed. Retry with a custom pip index URL?" "n"; then
        PIP_INDEX_URL="$(prompt_value "Enter pip index URL" "https://pypi.org/simple")"
        if ! install_requirements "${active_python}"; then
          log_error "Unable to install Python dependencies after retrying with ${PIP_INDEX_URL}."
          exit 1
        fi
      else
        log_error "Unable to install Python dependencies after multiple attempts."
        log_error "You can retry later with:"
        if [[ -n "${PIP_INDEX_URL}" ]]; then
          log_error "  ${active_python} -m pip install --no-cache-dir --prefer-binary --index-url ${PIP_INDEX_URL} -r ${ROOT_DIR}/requirements.txt"
        else
          log_error "  ${active_python} -m pip install --no-cache-dir --prefer-binary -r ${ROOT_DIR}/requirements.txt"
        fi
        exit 1
      fi
    fi

    if ! check_python_environment "${active_python}"; then
      log_error "Dependency installation completed but required modules are still missing."
      if [[ -n "${ENV_CHECK_MESSAGE}" ]]; then
        log_error "Environment check: ${ENV_CHECK_MESSAGE}"
      fi
      exit 1
    fi
  fi

  PYTHON_BIN="${active_python}"
}

parse_args() {
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --server-url)
        SERVER_URL="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --node-id)
        NODE_ID="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --camera-index)
        CAMERA_INDEX="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --fps)
        FPS="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --send-interval-ms)
        SEND_INTERVAL_MS="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --max-snapshots-per-second)
        MAX_SNAPSHOTS_PER_SECOND="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --yolo-every-n-frames)
        YOLO_EVERY_N_FRAMES="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --model-name)
        MODEL_NAME="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --objects-min-confidence)
        OBJECTS_MIN_CONFIDENCE="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --gestures-min-confidence)
        GESTURES_MIN_CONFIDENCE="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --delta-threshold)
        DELTA_THRESHOLD="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --pip-index-url)
        PIP_INDEX_URL="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --torch-index-url)
        TORCH_INDEX_URL="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --debug-ui)
        DEBUG_UI=1
        shift
        ;;
      --python)
        PYTHON_BIN="$(sanitize_value "${2:-}")"
        shift 2
        ;;
      --skip-install)
        SKIP_INSTALL=1
        shift
        ;;
      --non-interactive)
        NON_INTERACTIVE=1
        shift
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        log_error "Unknown option: $1"
        usage
        exit 1
        ;;
    esac
  done
}

collect_missing_inputs() {
  if [[ -z "${SERVER_URL}" ]]; then
    if [[ "${NON_INTERACTIVE}" -eq 1 ]]; then
      log_error "--server-url is required in non-interactive mode."
      exit 1
    fi
    SERVER_URL="$(prompt_value "Enter the WebSocket server URL from the main PC" "ws://MAIN-PC-IP:8765/vision")"
  fi
}

print_summary() {
  cat <<EOF
[INFO] Vision node launch configuration
  Python:                  ${PYTHON_BIN}
  Server URL:              ${SERVER_URL}
  Node ID:                 ${NODE_ID}
  Camera index:            ${CAMERA_INDEX}
  FPS:                     ${FPS}
  Send interval (ms):      ${SEND_INTERVAL_MS}
  Max snapshots/sec:       ${MAX_SNAPSHOTS_PER_SECOND}
  YOLO every N frames:     ${YOLO_EVERY_N_FRAMES}
  Model:                   ${MODEL_NAME}
  Objects min confidence:  ${OBJECTS_MIN_CONFIDENCE}
  Gestures min confidence: ${GESTURES_MIN_CONFIDENCE}
  Delta threshold:         ${DELTA_THRESHOLD}
  Torch index URL:         ${TORCH_INDEX_URL}
  Debug UI:                $( [[ "${DEBUG_UI}" -eq 1 ]] && echo "enabled" || echo "disabled" )
EOF
}

main() {
  local -a run_cmd=()

  parse_args "$@"
  ensure_python
  ensure_virtualenv
  ensure_requirements
  collect_missing_inputs
  print_summary

  cd "${ROOT_DIR}"
  run_cmd=(
    "${PYTHON_BIN}" "${ROOT_DIR}/main.py"
    --server-url "${SERVER_URL}"
    --node-id "${NODE_ID}"
    --camera-index "${CAMERA_INDEX}"
    --fps "${FPS}"
    --send-interval-ms "${SEND_INTERVAL_MS}"
    --max-snapshots-per-second "${MAX_SNAPSHOTS_PER_SECOND}"
    --yolo-every-n-frames "${YOLO_EVERY_N_FRAMES}"
    --model-name "${MODEL_NAME}"
    --objects-min-confidence "${OBJECTS_MIN_CONFIDENCE}"
    --gestures-min-confidence "${GESTURES_MIN_CONFIDENCE}"
    --delta-threshold "${DELTA_THRESHOLD}"
  )

  if [[ "${DEBUG_UI}" -eq 1 ]]; then
    run_cmd+=(--debug-ui)
  fi

  exec "${run_cmd[@]}"
}

main "$@"
