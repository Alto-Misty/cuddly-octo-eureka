#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h> // для `time` (не входит в stdlib)

#define SHM_SIZE 256

// Вспомогательная функция для преобразования числа в строку
void int_to_str(int num, char *buf, int *len) {
    if (num == 0) {
        buf[0] = '0';
        *len = 1;
        return;
    }
    int start = 0;
    if (num < 0) {
        buf[start++] = '-';
        num = -num;
    }
    int temp = num;
    int digits = 0;
    while (temp > 0) {
        temp /= 10;
        digits++;
    }
    int idx = start + digits;
    buf[idx] = '\0';
    idx--;
    while (num > 0) {
        buf[idx--] = '0' + (num % 10);
        num /= 10;
    }
    *len = start + digits;
}

int main() {
    char filename[256];
    write(STDOUT_FILENO, "Enter output filename: ", 23);
    int n = read(STDIN_FILENO, filename, 255);
    if (n <= 0) _exit(1);
    if (filename[n - 1] == '\n') filename[n - 1] = '\0';

    // Генерируем уникальные имена
    char shm_name[64] = "/my_shm_";
    char sem_empty_name[64] = "/sem_empty_";
    char sem_full_name[64] = "/sem_full_";

    time_t t = time(NULL); // разрешено, т.к. не из stdlib
    int suffix = (int)t % 10000; // ограничиваем длину

    int len;
    int_to_str(suffix, shm_name + 9, &len);
    shm_name[9 + len] = '\0';

    int_to_str(suffix, sem_empty_name + 11, &len);
    sem_empty_name[11 + len] = '\0';

    int_to_str(suffix, sem_full_name + 10, &len);
    sem_full_name[10 + len] = '\0';

    // Создаём shared memory
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) _exit(1);
    if (ftruncate(shm_fd, SHM_SIZE) == -1) _exit(1);

    char *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) _exit(1);

    // Создаём семафоры
    sem_t *sem_empty = sem_open(sem_empty_name, O_CREAT, 0666, 1); // начальное значение 1
    sem_t *sem_full = sem_open(sem_full_name, O_CREAT, 0666, 0);  // начальное значение 0

    pid_t pid = fork();
    if (pid == -1) _exit(1);

    if (pid == 0) {
        // Дочерний процесс
        char *argv[] = {"./child", filename, shm_name, sem_empty_name, sem_full_name, NULL};
        execve("./child", argv, NULL);
        _exit(127);
    } else {
        // Родитель
        write(STDOUT_FILENO, "Enter lines with integers (empty line to quit):\n", 49);

        char buffer[256];
        while (1) {
            write(STDOUT_FILENO, "> ", 2);
            int r = read(STDIN_FILENO, buffer, 255);
            if (r <= 0) break;
            if (r == 1 && buffer[0] == '\n') break;

            if (buffer[r - 1] != '\n') {
                buffer[r] = '\n';
                r++;
            }

            sem_wait(sem_empty); // ждём, пока память свободна

            for (int i = 0; i < r; ++i) {
                shm_ptr[i] = buffer[i];
            }
            shm_ptr[r] = '\0';

            sem_post(sem_full); // говорим ребёнку: "читай"
        }

        // Ожидаем завершения ребёнка
        wait(NULL);

        // Освобождаем ресурсы
        munmap(shm_ptr, SHM_SIZE);
        close(shm_fd);
        shm_unlink(shm_name);

        sem_close(sem_empty);
        sem_close(sem_full);
        sem_unlink(sem_empty_name);
        sem_unlink(sem_full_name);

        write(STDOUT_FILENO, "Parent finished.\n", 17);
    }

    return 0;
}