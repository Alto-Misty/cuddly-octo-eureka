#include <unistd.h> //системные вызовы
#include <fcntl.h>  //
#include <errno.h>  //

#define BUF_SIZE 256

// Простая функция для записи строки (для ошибок)
void write_str(int fd, const char *s) {
    while (*s) write(fd, s++, 1);
}

// Преобразует строку в целое (без проверки переполнения, как atoi)
int parse_int(const char *s, const char **end) {
    int val = 0;
    int neg = 0;
    if (*s == '-') {
        neg = 1;
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (*s - '0');
        s++;
    }
    *end = s;
    return neg ? -val : val;
}

// Пропускает пробелы
const char *skip_whitespace(const char *s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

int main(int argc, char *argv[]) {
    if (argc != 2) _exit(1);

    int out_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) _exit(1);

    char buffer[BUF_SIZE];
    while (1) {
        int r = read(STDIN_FILENO, buffer, BUF_SIZE - 1);
        if (r <= 0) break; //труба закрыта - работа оконч

        buffer[r] = '\0'; //конец строки для работы со строками
       
        //ручной разбор чисел
        const char *p = buffer; // указатель на начало строки
        int sum = 0;
        int found_number = 0;

        while (*p) { // пока не конец строки
            p = skip_whitespace(p);
            if (*p == '\n' || *p == '\0') break; //конец строки — выходим

            if ((*p >= '0' && *p <= '9') || *p == '-') {
                const char *end;
                int num = parse_int(p, &end);
                sum += num;
                found_number = 1;
                p = end;
            } else {
                // Пропускаем некорректные символы до следующего пробела
                while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
            }
        }

        if (found_number) {
            // Преобразуем sum в строку вручную
            char num_buf[16];
            int temp = sum;
            int i = 0;

            if (temp == 0) {
                num_buf[i++] = '0';
            } else {
                int neg = 0;
                if (temp < 0) {
                    neg = 1;
                    temp = -temp;
                }
                // Записываем цифры в обратном порядке
                while (temp > 0) {
                    num_buf[i++] = '0' + (temp % 10);
                    temp /= 10;
                }
                if (neg) num_buf[i++] = '-';
                // Разворачиваем
                for (int a = 0, b = i - 1; a < b; a++, b--) {
                    char t = num_buf[a];
                    num_buf[a] = num_buf[b];
                    num_buf[b] = t;
                }
            }
            num_buf[i++] = '\n';
            write(out_fd, num_buf, i);
        }
    }

    close(out_fd);
    _exit(0);
}