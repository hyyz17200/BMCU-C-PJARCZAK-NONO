#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

command -v pio >/dev/null 2>&1 || { echo "ERROR: nie ma 'pio' w PATH"; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "ERROR: nie ma 'python3' w PATH"; exit 1; }

OUT_DIR="dist/firmwares"
PIO_ENV="fw"

TXT_MODE="which_to_choose_mode.txt"
TXT_AUTOLOAD="which_to_choose_autoload.txt"
TXT_RGB="which_to_choose_filament_rgb.txt"
TXT_SLOTS="which_to_choose_slots.txt"
OUT_GUIDE="which_to_choose.txt"

[[ -f "${TXT_MODE}" ]]     || { echo "ERROR: brak ${TXT_MODE}"; exit 1; }
[[ -f "${TXT_AUTOLOAD}" ]] || { echo "ERROR: brak ${TXT_AUTOLOAD}"; exit 1; }
[[ -f "${TXT_RGB}" ]]      || { echo "ERROR: brak ${TXT_RGB}"; exit 1; }
[[ -f "${TXT_SLOTS}" ]]    || { echo "ERROR: brak ${TXT_SLOTS}"; exit 1; }

MODE_A1_DIR="standard(A1)"
MODE_P1S_DIR="high_force_load(P1S)"

SOLO_RETRACT="0.095f"
RETRACTS=(
  "0.10"
  "0.20" "0.25" "0.30" "0.35" "0.40"
  "0.45" "0.50" "0.55" "0.60" "0.65"
  "0.70" "0.75" "0.80" "0.85" "0.90"
)

build_and_copy() {
  local out_path="$1"
  local ams_num="$2"
  local retract_len="$3"
  local dm="$4"
  local rgb="$5"
  local p1s="$6"

  echo "=== BUILD: P1S=${p1s} DM=${dm} RGB=${rgb} AMS_NUM=${ams_num} RETRACT=${retract_len} -> ${out_path}"

  BAMBU_BUS_AMS_NUM="${ams_num}" \
  AMS_RETRACT_LEN="${retract_len}" \
  BMCU_DM_TWO_MICROSWITCH="${dm}" \
  BMCU_ONLINE_LED_FILAMENT_RGB="${rgb}" \
  DBMCU_P1S="${p1s}" \
  BMCU_SOFT_LOAD="0" \
  pio run -e "${PIO_ENV}"

  local src="${PLATFORMIO_BUILD_DIR:-.pio/build}/${PIO_ENV}/firmware.bin"
  [[ -f "${src}" ]] || { echo "ERROR: brak ${src}"; exit 1; }

  mkdir -p "$(dirname "${out_path}")"
  cp -f "${src}" "${out_path}"
}

# Only this checkout's dist/firmwares can be published. A failed build leaves it intact.
STAGE_DIR="$(python3 scripts/firmware_output.py begin)"
trap 'python3 scripts/firmware_output.py cleanup "${STAGE_DIR}"' EXIT

cp -f "${TXT_MODE}" "${STAGE_DIR}/${OUT_GUIDE}"

for p1s in 0 1; do
  if [[ "${p1s}" == "1" ]]; then
    mode_dir="${MODE_P1S_DIR}"
  else
    mode_dir="${MODE_A1_DIR}"
  fi

  mode_base="${STAGE_DIR}/${mode_dir}"
  mkdir -p "${mode_base}"
  cp -f "${TXT_AUTOLOAD}" "${mode_base}/${OUT_GUIDE}"

  for dm in 1 0; do
    if [[ "${dm}" == "1" ]]; then
      dm_dir="AUTOLOAD"
    else
      dm_dir="NO_AUTOLOAD"
    fi

    dm_base="${mode_base}/${dm_dir}"
    mkdir -p "${dm_base}"
    cp -f "${TXT_RGB}" "${dm_base}/${OUT_GUIDE}"

    for rgb in 1 0; do
      if [[ "${rgb}" == "1" ]]; then
        rgb_dir="FILAMENT_RGB_ON"
      else
        rgb_dir="FILAMENT_RGB_OFF"
      fi

      base="${dm_base}/${rgb_dir}"
      mkdir -p "${base}"/{SOLO,AMS_A,AMS_B,AMS_C,AMS_D}
      cp -f "${TXT_SLOTS}" "${base}/${OUT_GUIDE}"

      build_and_copy "${base}/SOLO/solo_${SOLO_RETRACT}.bin" 0 "${SOLO_RETRACT}" "${dm}" "${rgb}" "${p1s}"

      for slot in A B C D; do
        case "${slot}" in
          A) ams_num=0 ;;
          B) ams_num=1 ;;
          C) ams_num=2 ;;
          D) ams_num=3 ;;
        esac

        for r in "${RETRACTS[@]}"; do
          build_and_copy \
            "${base}/AMS_${slot}/ams_${slot,,}_${r}f.bin" \
            "${ams_num}" \
            "${r}f" \
            "${dm}" \
            "${rgb}" \
            "${p1s}"
        done
      done
    done
  done
done

python3 scripts/firmware_output.py publish "${STAGE_DIR}"
trap - EXIT

echo
echo "DONE. Wyniki w: ${OUT_DIR}/"
echo "Manifest: ${OUT_DIR}/manifest.txt"
