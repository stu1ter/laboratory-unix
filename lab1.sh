#!/bin/sh

# Коды ошибок:
#   1 — ошибка в аргументах
#   2 — исходный файл не найден
#   3 — ошибка в комментарии Output
#   4 — не найдена необходимая утилита
#   5 — ошибка сборки
#   6 — ошибка временного каталога
#   7 — завершение по сигналу SIGHUP
#   8 — завершение по сигналу SIGINT
#   9 — завершение по сигналу SIGQUIT
#   10 — завершение по сигналу SIGTERM

if [ "$#" -ne 1 ]; then
    echo "Должен быть указан один аргумент — путь к исходному файлу" >&2
    exit 1
fi

src=$1

if [ ! -f "$src" ]; then
    echo "Исходный файл не найден" >&2
    exit 2
fi

src_dir=$(dirname "$src")
src_base=$(basename "$src")

out=$(
    sed -n '/Output:/ {s/.*Output:[[:space:]]*//p;q}' "$src" |
    sed 's/[[:space:]].*$//'
)

out=$(
    sed -n '
        /^[[:space:]]*\(\/\/\|\/\*\|%\|#\|\*\)/ {
            /Output:/ {
                s/.*Output:[[:space:]]*//
                s/[[:space:]].*$//
                p
                q
            }
        }
    ' "$src"
)

case "$out" in
    */*|.|..)
        echo "Недопустимое имя: $out" >&2
        exit 3
        ;;
esac

if [ -z "$out" ]; then
    echo "Комментарий Output не найден" >&2
    exit 3
fi

if [ "$out" = "$src_base" ]; then
    echo "Имя выходного файла совпадает с именем исходного файла" >&2
    exit 3
fi

tmpdir=$(mktemp -d) || {
    echo "Не удалось создать временный каталог" >&2
    exit 6
}

cleanup() {
    status=$?
    trap - 0 HUP INT QUIT TERM

    if [ -n "$tmpdir" ] && [ -d "$tmpdir" ]; then
        rm -rf "$tmpdir"
    fi

    exit "$status"
}

trap cleanup 0
trap 'exit 7' HUP
trap 'exit 8' INT
trap 'exit 9' QUIT
trap 'exit 10' TERM

case "$src" in
    *.c) kind=c ;;
    *.cc|*.cpp|*.cxx|*.C) kind=cxx ;;
    *.tex) kind=tex ;;
    *)
        echo "Не поддерживаемый тип исходника" >&2
        exit 2
        ;;
esac

cd "$src_dir" || {
    echo "Не удалось перейти в каталог исходников" >&2
    exit 6
}

src_local="./$src_base"

case "$kind" in
    c)
        command -v cc >/dev/null 2>&1 || {
            echo "Компилятор C не найден" >&2
            exit 4
        }

        cc "$src_local" -o "$tmpdir/$out"

        built=$out
        ;;

    cxx)
        command -v c++ >/dev/null 2>&1 || {
            echo "Компилятор C++ не найден" >&2
            exit 4
        }

        c++ "$src_local" -o "$tmpdir/$out"

        built=$out
        ;;

    tex)
        command -v pdflatex >/dev/null 2>&1 || {
            echo "Ошибка: движок TeX не найден" >&2
            exit 4
        }

        case "$out" in
            *.pdf) ;;
            *) echo "Output должен быть .pdf" >&2; exit 3 ;;
        esac

        pdflatex \
            -interaction=nonstopmode \
            -halt-on-error \
            -output-directory="$tmpdir" \
            -jobname="${out%.pdf}" \
            "$src_local" \
            >/dev/null

        built=$out
        ;;
esac

if [ ! -f "$tmpdir/$built" ]; then
    echo "Ошибка сборки, файл не создан" >&2
    exit 5
fi

mv -f "$tmpdir/$built" "./$out" || {
    echo "Ошибка: не удалось переместить файл" >&2
    exit 6
}

exit 0
