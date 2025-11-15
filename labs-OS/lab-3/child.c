#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>

#define SHM_SIZE 256

int main(int argc, char *argv[]) {
    if (argc != 5) _exit(1);

    char *filename = argv[1];
    char *shm_name = argv[2];
    char *sem_empty_name = argv[3];
    char *sem_full_name = argv[4];

    // Открываем shared memory
    int shm_fd = shm_open(shm_name, O_RDONLY, 0666);
    if (shm_fd == -1) _exit(1);

    char *shm_ptr = mmap(NULL, SHM_SIZE, PROT_READ, MAP_SHARED, shm_fd, 0);
    if (shm_ptr == MAP_FAILED) _exit(1);

    // Открываем семафоры
    sem_t *sem_empty = sem_open(sem_empty_name, 0);
    sem_t *sem_full = sem_open(sem_full_name, 0);

    if (sem_empty == SEM_FAILED || sem_full == SEM_FAILED) _exit(1);

    int out_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out_fd == -1) _exit(1);

    while (1) {
        sem_wait(sem_full); // ждём, пока родитель запишет

        // Читаем из общей памяти
        const char *p = shm_ptr;
        int sum = 0;
        int neg = 0;
        int num = 0;
        int found = 0;

        while (*p && *p != '\n') {
            if (*p == '-' && !found) {
                neg = 1;
                p++;
            } else if (*p >= '0' && *p <= '9') {
                num = num * 10 + (*p - '0');
                found = 1;
            } else if (*p == ' ') {
                sum += (neg ? -num : num);
                num = 0;
                neg = 0;
                found = 0;
            }
            p++;
        }
        if (found) {
            sum += (neg ? -num : num);
        }

        // Пишем результат в файл
        char s[32];
        int i = 0;
        int temp = sum;
        if (temp == 0) s[i++] = '0';
        else {
            if (temp < 0) {
                s[i++] = '-';
                temp = -temp;
            }
            int start = i;
            while (temp > 0) {
                s[i++] = '0' + (temp % 10);
                temp /= 10;
            }
            for (int a = start, b = i - 1; a < b; a++, b--) {
                char t = s[a];
                s[a] = s[b];
                s[b] = t;
            }
        }
        s[i++] = '\n';
        write(out_fd, s, i);

        sem_post(sem_empty); // говорим родителю: "готово"
    }

    close(out_fd);
    munmap(shm_ptr, SHM_SIZE);
    close(shm_fd);

    sem_close(sem_empty);
    sem_close(sem_full);

    _exit(0);
}