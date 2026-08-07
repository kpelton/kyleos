#include <stdio.h>

#define WIDTH 80
#define HEIGHT 25
#define PADDLE_LEN 10
#define BRICK_ROWS 5
#define FRAME_MS 40
#define BALL_FRAME_DIVIDER 3

static void raw_mode(int enabled)
{
    __asm__ volatile("mov $27, %%rax; int $0x80" : : "D"(enabled) : "rax", "memory");
}

static void sleep_ms(int milliseconds)
{
    __asm__ volatile("mov $0, %%rax; int $0x80" : : "D"(milliseconds) : "rax", "memory");
}

/* Syscall 10 returns a queued raw character, or '\0' when none is pending. */
static char poll_char(void)
{
    char input[2] = { 0, 0 };
    __asm__ volatile("mov $10, %%rax; int $0x80" : : "D"(input) : "rax", "memory");
    return input[0];
}

static void clear_board(char board[HEIGHT][WIDTH])
{
    for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++) board[y][x] = ' ';
}

static int bricks_left(char bricks[BRICK_ROWS][WIDTH])
{
    for (int y = 0; y < BRICK_ROWS; y++)
        for (int x = 1; x < WIDTH - 1; x++) if (bricks[y][x]) return 1;
    return 0;
}

static void draw(char board[HEIGHT][WIDTH], char bricks[BRICK_ROWS][WIDTH],
                 int paddle, int ball_x, int ball_y, int score)
{
    clear_board(board);
    for (int x = 0; x < WIDTH; x++) board[0][x] = board[HEIGHT - 1][x] = '#';
    for (int y = 1; y < HEIGHT - 1; y++) board[y][0] = board[y][WIDTH - 1] = '#';
    for (int y = 0; y < BRICK_ROWS; y++)
        for (int x = 1; x < WIDTH - 1; x++) if (bricks[y][x]) board[y + 2][x] = '=';
    for (int x = 0; x < PADDLE_LEN; x++) board[HEIGHT - 2][paddle + x] = '_';
    board[ball_y][ball_x] = 'o';
    printf("\033[Hscore:%d a/d move q quit\r\n", score);
    for (int y = 1; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) putchar(board[y][x]);
        printf("\r\n");
    }
    fflush(stdout);
}

static char scene_char(char bricks[BRICK_ROWS][WIDTH], int paddle, int x, int y)
{
    if (y == 0 || y == HEIGHT - 1 || x == 0 || x == WIDTH - 1) return '#';
    if (y >= 2 && y < BRICK_ROWS + 2 && bricks[y - 2][x]) return '=';
    if (y == HEIGHT - 2 && x >= paddle && x < paddle + PADDLE_LEN) return '_';
    return ' ';
}

/* Board row y appears one terminal row below the score line. */
static void put_at(int x, int y, char ch)
{
    printf("\033[%d;%dH%c", y + 1, x + 1, ch);
}

int main(void)
{
    char board[HEIGHT][WIDTH], bricks[BRICK_ROWS][WIDTH] = {{0}};
    int paddle = (WIDTH - PADDLE_LEN) / 2, ball_x = WIDTH / 2, ball_y = HEIGHT - 3;
    int vx = 1, vy = -1, score = 0, running = 1, ball_frame = 0;

    for (int y = 0; y < BRICK_ROWS; y++)
        for (int x = 1; x < WIDTH - 1; x++) bricks[y][x] = 1;
    raw_mode(1);
    printf("\033[2J\033[?25l");
    draw(board, bricks, paddle, ball_x, ball_y, score);
    while (running) {
        int old_paddle = paddle;
        int old_ball_x = ball_x;
        int old_ball_y = ball_y;
        int brick_hit = 0;
        char key = poll_char();
        if ((key == 'a' || key == 'A') && paddle > 1) paddle--;
        else if ((key == 'd' || key == 'D') && paddle + PADDLE_LEN < WIDTH - 1) paddle++;
        else if (key == 'q' || key == 'Q') break;
        ball_frame++;
        if (ball_frame >= BALL_FRAME_DIVIDER) {
            ball_frame = 0;
            ball_x += vx; ball_y += vy;
            if (ball_x <= 1 || ball_x >= WIDTH - 2) { vx = -vx; ball_x += 2 * vx; }
            if (ball_y <= 1) { vy = -vy; ball_y += 2 * vy; }
            if (ball_y >= HEIGHT - 2) {
                if (ball_x >= paddle && ball_x < paddle + PADDLE_LEN) {
                    vy = -1; vx = ball_x < paddle + PADDLE_LEN / 2 ? -1 : 1; ball_y = HEIGHT - 4;
                } else running = 0;
            }
            if (ball_y >= 2 && ball_y < BRICK_ROWS + 2 && bricks[ball_y - 2][ball_x]) {
                bricks[ball_y - 2][ball_x] = 0; vy = -vy; score++; brick_hit = 1;
            }
            put_at(old_ball_x, old_ball_y,
                   scene_char(bricks, paddle, old_ball_x, old_ball_y));
            put_at(ball_x, ball_y, 'o');
        }
        if (paddle < old_paddle) {
            put_at(old_paddle + PADDLE_LEN - 1, HEIGHT - 2, ' ');
            put_at(paddle, HEIGHT - 2, '_');
        } else if (paddle > old_paddle) {
            put_at(old_paddle, HEIGHT - 2, ' ');
            put_at(paddle + PADDLE_LEN - 1, HEIGHT - 2, '_');
        }
        if (brick_hit) printf("\033[1;1Hscore:%d a/d move q quit", score);
        fflush(stdout);
        if (!bricks_left(bricks)) running = 0;
        sleep_ms(FRAME_MS);
    }
    raw_mode(0);
    printf("\033[?25h\033[2J\033[Hbreakout score:%d\r\n", score);
    return 0;
}
