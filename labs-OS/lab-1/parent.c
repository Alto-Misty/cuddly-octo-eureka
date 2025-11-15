#include <unistd.h>   //системные вызовы
#include <sys/wait.h> //ожидалка
#include <fcntl.h>    //для open()
#include <errno.h>    //для ошибок

#define BUF_SIZE 256 //размер буфера

int main() {
    char filename[BUF_SIZE];
    write(STDOUT_FILENO, "Enter output filename: ", 23); //куда записать результат
    
    int n = read(STDIN_FILENO, filename, BUF_SIZE - 1); //Слушает ответ, записывает в массив
    if (n <= 0) _exit(1);
    if (filename[n - 1] == '\n') filename[n - 1] = '\0'; //уборка символа символа новой строки
    else filename[n] = '\0';

    int pipe1[2]; // parent -> child
    int pipe2[2]; // child -> parent (optional)

    if (pipe(pipe1) == -1 || pipe(pipe2) == -1) _exit(1); //проверка

    pid_t pid = fork(); //копия - созд доч процесса
    if (pid == -1) _exit(1);

    if (pid == 0) {
        // Дочерний процесс
        close(pipe1[1]); //закрываем запись в pipe1
        close(pipe2[0]); //закрываем чтение из pipe2

        dup2(pipe1[0], STDIN_FILENO); //stdin из родителя а не с клавы
        close(pipe1[0]);

        dup2(pipe2[1], STDOUT_FILENO);//вывод в родителя 
        close(pipe2[1]);

        char *argv[] = {"./child", filename, NULL}; //прога child.c вместо копии предка
        execve("./child", argv, NULL);
        // Если execve вернулся — ошибка
        _exit(127);
    } else {
        // Родитель
        close(pipe1[0]); // не читаем из pipe1
        close(pipe2[1]); // не пишем в pipe2

        write(STDOUT_FILENO, "Enter lines with integers (empty line to quit):\n", 49);

        char buffer[BUF_SIZE];
        while (1) {
            write(STDOUT_FILENO, "> ", 2);
            int r = read(STDIN_FILENO, buffer, BUF_SIZE - 1);
            if (r <= 0) break; //Больше нет ввода, или что-то сломалось
            if (r == 1 && buffer[0] == '\n') break; //просто Enter

            // Убедимся, что строка заканчивается \n
            if (buffer[r - 1] != '\n') {
                buffer[r] = '\n'; //если нет добавляем
                r++; //длинаcтроки увеличилась
            }
            if (write(pipe1[1], buffer, r) != r) break; //все лли байты записал write
        }

        close(pipe1[1]);
        wait(NULL); //ожидание завершения
        write(STDOUT_FILENO, "Parent finished.\n", 18);
    }

    return 0;
}