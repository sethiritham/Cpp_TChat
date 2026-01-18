#include <ncurses.h>

int main()
{
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    int height, width;
    getmaxyx(stdscr, height, width);


    mvprintw(height/2, (width-11)/2, "Hello World");
    refresh();


    getch();
    endwin();
    return 0;

}                       