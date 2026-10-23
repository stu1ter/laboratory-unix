#!/bin/sh
set -eu

VOLUME_DIR="/data"
LOCK_FILE="${VOLUME_DIR}/.lock"

CONTAINER_ID=$(hostname)
echo "Контейнер запущен. ID: ${CONTAINER_ID}"

local_counter=0

while true; do
    FILE_NAME=""
    TARGET_FILE=""

    exec 9>>"$LOCK_FILE"
    if ! flock -w 30 -x 9; then
        echo "Не удалось получить блокировку" >&2
        exec 9>&-
        sleep 1
        continue
    fi

    i=1
    while true; do
        FILE_NAME=$(printf "%03d" "$i")
        TARGET_FILE="${VOLUME_DIR}/${FILE_NAME}"

        if [ ! -e "$TARGET_FILE" ]; then
            local_counter=$((local_counter + 1))
            echo "${CONTAINER_ID} ${local_counter}" > "$TARGET_FILE"
            break
        fi
        i=$((i + 1))
    done

    exec 9>&-

    echo "Создан файл: ${FILE_NAME} (Запись: ${CONTAINER_ID} ${local_counter})"

    sleep 1

    if [ -e "$TARGET_FILE" ]; then
        rm -f "$TARGET_FILE"
        echo "Удален файл: ${FILE_NAME}"
    fi

    sleep 1
done
