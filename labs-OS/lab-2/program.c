#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>

// Структура точки
typedef struct { double x; double y; double z; } tpoint;

// Глобальные переменные
tpoint *points = NULL;
int point_count = 0;
double max_area = -1.0;
tpoint best_triangle[3];
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// Структура для аргументов потока
typedef struct { int start; int end; } tthread_args;

// Вспомогательная функция для вычисления квадратного корня (итерационный метод Ньютона)
double my_sqrt(double value) {
    if (value <= 0.0) return 0.0;
    double guess = value;
    double last_guess;
    do {
        last_guess = guess;
        guess = (guess + value / guess) * 0.5;
    } while ((guess < last_guess) && ((last_guess - guess) > 1e-10));
    return guess;
}

// Функция для вычисления площади треугольника
double calculate_triangle_area(tpoint a, tpoint b, tpoint c) {
    double abx = b.x - a.x;
    double aby = b.y - a.y;
    double abz = b.z - a.z;

    double acx = c.x - a.x;
    double acy = c.y - a.y;
    double acz = c.z - a.z;

    // Векторное произведение
    double crossx = aby * acz - abz * acy;
    double crossy = abz * acx - abx * acz;
    double crossz = abx * acy - aby * acx;

    // Квадрат длины вектора (квадрат нормы)
    double length_sq = crossx * crossx + crossy * crossy + crossz * crossz;
    if (length_sq <= 0.0) return 0.0; // Вырожденный треугольник

    // Площадь = 0.5 * |векторное произведение|
    return 0.5 * my_sqrt(length_sq);
}

// Функция, которую выполняет поток
void *process_triangles(void *arg) {
    tthread_args *args = (tthread_args *)arg;

    // Проверка, чтобы поток не начал обработку, если ему не досталось точек
    if (args->start >= point_count) return NULL;

    // Ограничиваем конец, если он выходит за пределы
    int end_limit = (args->end > point_count) ? point_count : args->end;

    for (int i = args->start; i < end_limit; i++) {
        for (int j = i + 1; j < point_count; j++) {
            for (int k = j + 1; k < point_count; k++) {
                double area = calculate_triangle_area(points[i], points[j], points[k]);

                pthread_mutex_lock(&mutex);
                if (area > max_area) {
                    max_area = area;
                    best_triangle[0] = points[i];
                    best_triangle[1] = points[j];
                    best_triangle[2] = points[k];
                }
                pthread_mutex_unlock(&mutex);
            }
        }
    }

    return NULL;
}

// Последовательная версия
void sequential_version() {
    max_area = -1.0;
    for (int i = 0; i < point_count; i++) {
        for (int j = i + 1; j < point_count; j++) {
            for (int k = j + 1; k < point_count; k++) {
                double area = calculate_triangle_area(points[i], points[j], points[k]);
                if (area > max_area) {
                    max_area = area;
                    best_triangle[0] = points[i];
                    best_triangle[1] = points[j];
                    best_triangle[2] = points[k];
                }
            }
        }
    }
}

// Функция для получения длины строки (вместо strlen)
int my_strlen(const char *str) {
    int len = 0;
    while (str[len] != '\0') len++;
    return len;
}

// Преобразование числа в строку (вместо snprintf)
void double_to_string(double value, char *buffer, int buffer_size) {
    int integer_part = (int)value;
    double fraction_part = value - integer_part;
    if (fraction_part < 0) fraction_part = -fraction_part;

    int i = 0;
    if (integer_part == 0) {
        buffer[i++] = '0';
    } else {
        char temp[32];
        int j = 0;
        int temp_int = integer_part;
        int is_negative = (temp_int < 0);
        if (is_negative) temp_int = -temp_int;
        if (is_negative) buffer[i++] = '-';
        while (temp_int > 0 && j < 31) {
            temp[j++] = '0' + (temp_int % 10);
            temp_int /= 10;
        }
        for (int k = j - 1; k >= 0 && i < buffer_size - 1; k--) {
            buffer[i++] = temp[k];
        }
    }

    if (i < buffer_size - 1) buffer[i++] = '.';

    fraction_part *= 1000000;
    int frac_int = (int)fraction_part;
    char frac_temp[8];
    int j = 0;
    while (j < 6 && i < buffer_size - 1) {
        frac_temp[j++] = '0' + (frac_int % 10);
        frac_int /= 10;
    }
    for (int k = 5; k >= 0 && i < buffer_size - 1; k--) {
        buffer[i++] = frac_temp[k];
    }

    if (i < buffer_size) buffer[i] = '\0';
}

// Преобразование int в строку
void int_to_string(int value, char *buffer, int buffer_size) {
    if (value == 0) {
        if (buffer_size > 1) { buffer[0] = '0'; buffer[1] = '\0'; }
        return;
    }
    int neg = (value < 0);
    if (neg) value = -value; // Это может быть небезопасно для INT_MIN, но упрощает логику
    char temp[16];
    int i = 0;
    while (value > 0 && i < 15) {
        temp[i++] = '0' + (value % 10);
        value /= 10;
    }
    int j = 0;
    if (neg && j < buffer_size - 1) buffer[j++] = '-';
    for (int k = i - 1; k >= 0 && j < buffer_size - 1; k--) {
        buffer[j++] = temp[k];
    }
    if (j < buffer_size) buffer[j] = '\0';
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        write(STDERR_FILENO, "Usage: ./program <threads> <input> <output>\n", 42);
        return 1;
    }

    // Парсинг числа потоков
    int thread_count = 0;
    char *p = argv[1];
    while (*p >= '0' && *p <= '9') thread_count = thread_count * 10 + (*p++ - '0');
    if (thread_count <= 0) {
        write(STDERR_FILENO, "Error: Thread count must be positive.\n", 37);
        return 1;
    }

    // Открываем и мапим файл
    int fd = open(argv[2], O_RDONLY);
    if (fd == -1) { write(STDERR_FILENO, "Cannot open input file.\n", 24); return 1; }
    struct stat sb;
    if (fstat(fd, &sb) == -1) { close(fd); write(STDERR_FILENO, "Cannot stat input file.\n", 24); return 1; }
    char *file_data = (char *)mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_data == MAP_FAILED) { close(fd); write(STDERR_FILENO, "Cannot mmap input file.\n", 24); return 1; }
    close(fd);

    // Подсчёт количества точек
    point_count = 0;
    for (off_t i = 0; i < sb.st_size; i++) if (file_data[i] == '\n') point_count++;

    // Проверка на минимальное количество точек для треугольника
    if (point_count < 3) {
        write(STDERR_FILENO, "Error: Not enough points in the file (minimum 3 required).\n", 56);
        munmap(file_data, sb.st_size);
        return 1;
    }

    // Выделяем память через mmap
    points = (tpoint *)mmap(NULL, point_count * sizeof(tpoint), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (points == MAP_FAILED) {
        write(STDERR_FILENO, "Cannot allocate memory for points.\n", 36);
        munmap(file_data, sb.st_size);
        return 1;
    }

    // Парсинг точек
    int index = 0;
    char *line_start = file_data;
    for (off_t i = 0; i <= sb.st_size; i++) {
        if (file_data[i] == '\n' || i == sb.st_size) {
            if (line_start >= file_data + sb.st_size) break;

            char *current_p = line_start;
            double vals[3] = {0, 0, 0};
            int val_idx = 0;

            while (current_p < file_data + sb.st_size && *current_p != '\n' && val_idx < 3) {
                while (*current_p == ' ' || *current_p == '\t') current_p++;
                if (*current_p == '\n' || current_p >= file_data + sb.st_size) break;

                int neg = (*current_p == '-');
                if (neg) current_p++;

                double num = 0.0;
                int dec = 0;
                double frac_mult = 1.0;

                while ((*current_p >= '0' && *current_p <= '9') || *current_p == '.') {
                    if (*current_p == '.') {
                        if (dec) break; // Вторая точка - ошибка
                        dec = 1;
                        current_p++;
                        continue;
                    }
                    if (!dec) {
                        num = num * 10.0 + (*current_p - '0');
                    } else {
                        frac_mult /= 10.0;
                        num += frac_mult * (*current_p - '0');
                    }
                    current_p++;
                }

                vals[val_idx] = neg ? -num : num;
                val_idx++;
                while (*current_p != ' ' && *current_p != '\t' && *current_p != '\n' && current_p < file_data + sb.st_size) current_p++;
            }

            if (val_idx == 3) { // Успешно прочитали 3 координаты
                points[index].x = vals[0];
                points[index].y = vals[1];
                points[index].z = vals[2];
                index++;
            }
            line_start = file_data + i + 1;
        }
    }
    point_count = index; // Обновляем, если последняя строка не пустая

    // --- Основная логика ---
    struct timespec start, end;

    // Замер времени для последовательной версии
    clock_gettime(CLOCK_MONOTONIC, &start);
    sequential_version();
    clock_gettime(CLOCK_MONOTONIC, &end);
    double sequential_time = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    // Замер времени для параллельной версии
    clock_gettime(CLOCK_MONOTONIC, &start);

    pthread_t *threads = (pthread_t *)mmap(NULL, thread_count * sizeof(pthread_t), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (threads == MAP_FAILED) { write(STDERR_FILENO, "Cannot allocate memory for threads.\n", 37); goto cleanup_and_exit; }
    tthread_args *args = (tthread_args *)mmap(NULL, thread_count * sizeof(tthread_args), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (args == MAP_FAILED) { write(STDERR_FILENO, "Cannot allocate memory for args.\n", 34); goto cleanup_and_exit; }

    int chunk_size = point_count / thread_count;
    for (int t = 0; t < thread_count; t++) {
        args[t].start = t * chunk_size;
        args[t].end = (t == thread_count - 1) ? point_count : (t + 1) * chunk_size;
        if (pthread_create(&threads[t], NULL, process_triangles, &args[t]) != 0) {
             write(STDERR_FILENO, "Error creating thread.\n", 23);
             goto cleanup_and_exit; // Переход к централизованной очистке
        }
    }

    for (int t = 0; t < thread_count; t++) pthread_join(threads[t], NULL);

    clock_gettime(CLOCK_MONOTONIC, &end);
    double elapsed_ms = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;

    // Запись результата в файл
    int out_fd = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) { write(STDERR_FILENO, "Cannot open output file.\n", 25); goto cleanup_and_exit; }

    char temp[64];
    double_to_string(best_triangle[0].x, temp, 64); write(out_fd, temp, my_strlen(temp)); write(out_fd, " ", 1);
    double_to_string(best_triangle[0].y, temp, 64); write(out_fd, temp, my_strlen(temp)); write(out_fd, " ", 1);
    double_to_string(best_triangle[0].z, temp, 64); write(out_fd, temp, my_strlen(temp)); write(out_fd, "\n", 1);
    double_to_string(best_triangle[1].x, temp, 64); write(out_fd, temp, my_strlen(temp)); write(out_fd, " ", 1);
    double_to_string(best_triangle[1].y, temp, 64); write(out_fd, temp, my_strlen(temp)); write(out_fd, " ", 1);
    double_to_string(best_triangle[1].z, temp, 64); write(out_fd, temp, my_strlen(temp)); write(out_fd, "\n", 1);
    double_to_string(best_triangle[2].x, temp, 64); write(out_fd, temp, my_strlen(temp)); write(out_fd, " ", 1);
    double_to_string(best_triangle[2].y, temp, 64); write(out_fd, temp, my_strlen(temp)); write(out_fd, " ", 1);
    double_to_string(best_triangle[2].z, temp, 64); write(out_fd, temp, my_strlen(temp)); write(out_fd, "\n", 1);
    write(out_fd, "Area: ", 6);
    char area_str[32];
    double_to_string(max_area, area_str, 32);
    write(out_fd, area_str, my_strlen(area_str));
    write(out_fd, "\n", 1);
    close(out_fd);

    // Вывод времени выполнения (в stderr)
    char time_result[256];
    char time_str[32];
    double_to_string(elapsed_ms, time_str, 32);

    int len = 0;
    const char *prefix = "Parallel time (with ";
    const char *middle = " threads): ";
    const char *suffix = " ms\n";
    int pl = my_strlen(prefix); for (int i = 0; i < pl && len < 255; i++) time_result[len++] = prefix[i];
    char thread_count_str[16]; int_to_string(thread_count, thread_count_str, 16);
    int tcl = my_strlen(thread_count_str); for (int i = 0; i < tcl && len < 255; i++) time_result[len++] = thread_count_str[i];
    int ml = my_strlen(middle); for (int i = 0; i < ml && len < 255; i++) time_result[len++] = middle[i];
    int tsl = my_strlen(time_str); for (int i = 0; i < tsl && len < 255; i++) time_result[len++] = time_str[i];
    int sl = my_strlen(suffix); for (int i = 0; i < sl && len < 255; i++) time_result[len++] = suffix[i];
    time_result[len] = '\0';
    write(STDERR_FILENO, time_result, len);

    char seq_time_str[32];
    double_to_string(sequential_time, seq_time_str, 32);
    const char *seq_prefix = "Sequential time: ";
    const char *seq_suffix = " ms\n";
    len = 0;
    int spl = my_strlen(seq_prefix); for (int i = 0; i < spl && len < 255; i++) time_result[len++] = seq_prefix[i];
    int stsl = my_strlen(seq_time_str); for (int i = 0; i < stsl && len < 255; i++) time_result[len++] = seq_time_str[i];
    int ssl = my_strlen(seq_suffix); for (int i = 0; i < ssl && len < 255; i++) time_result[len++] = seq_suffix[i];
    time_result[len] = '\0';
    write(STDERR_FILENO, time_result, len);

    // Освобождение памяти
cleanup_and_exit:
    munmap(file_data, sb.st_size);
    munmap(points, point_count * sizeof(tpoint));
    if (threads) munmap(threads, thread_count * sizeof(pthread_t));
    if (args) munmap(args, thread_count * sizeof(tthread_args));
    pthread_mutex_destroy(&mutex);

    return 0;
}