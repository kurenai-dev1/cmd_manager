#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_CMDS 200
#define VIEW_WINDOW 15
#define CMD_LEN 1024
#define ALL_HIST_MAX 5000

typedef struct {
    char cmd[CMD_LEN];
} CmdItem;

void clear_screen() {
    printf("\033[H\033[2J");
    fflush(stdout);
}

void get_list_path(char *path, size_t max_len) {
    snprintf(path, max_len, "%s/.cmd_history_list", getenv("HOME"));
}

void get_bash_history_path(char *path, size_t max_len) {
    snprintf(path, max_len, "%s/.bash_history", getenv("HOME"));
}

void save_all_items(CmdItem *items, int count, int sort_abc) {
    char list_path[512];
    get_list_path(list_path, sizeof(list_path));
    FILE *fp = fopen(list_path, "w");
    if (fp) {
        if (!sort_abc) {
            for (int i = count - 1; i >= 0; i--) {
                fprintf(fp, "%s\n", items[i].cmd);
            }
        } else {
            for (int i = 0; i < count; i++) {
                fprintf(fp, "%s\n", items[i].cmd);
            }
        }
        fclose(fp);
    }
}

int compare_abc(const void *a, const void *b) {
    return strcmp(((CmdItem *)a)->cmd, ((CmdItem *)b)->cmd);
}

int get_linux_key() {
    char buf[8];
    int n = read(STDIN_FILENO, buf, sizeof(buf));
    if (n <= 0) return 0;

    if (buf[0] == 27) {
        if (n == 1) return 27;
        if (buf[1] == 91) {
            if (buf[2] == 65) return 1001;
            if (buf[2] == 66) return 1002;
            if (buf[2] == 51 && buf[3] == 126) return 1003;
        }
        return 0;
    }
    return buf[0];
}

// ==========================================
// 1. addcmd
// ==========================================
void do_addcmd() {
    char bash_hist_path[512];
    get_bash_history_path(bash_hist_path, sizeof(bash_hist_path));

    FILE *fp = fopen(bash_hist_path, "r");
    if (!fp) {
        printf("エラー: .bash_history が開けませんでした。\n");
        return;
    }

    static char all_lines[ALL_HIST_MAX][CMD_LEN];
    int total_lines = 0;
    char line[CMD_LEN];

    while (fgets(line, sizeof(line), fp) && total_lines < ALL_HIST_MAX) {
        line[strcspn(line, "\r\n")] = '\0';

        if (strlen(line) == 0 || line[0] == '#') {
            continue;
        }

        if (strncmp(line, "mycmd", 5) == 0 ||
            strncmp(line, "./mycmd", 7) == 0 ||
            strcmp(line, "addcmd") == 0 ||
            strcmp(line, "selcmd") == 0) {
            continue;
        }

        strncpy(all_lines[total_lines], line, CMD_LEN);
        total_lines++;
    }
    fclose(fp);

    CmdItem history[MAX_CMDS];
    int h_count = 0;

    for (int i = total_lines - 1; i >= 0; i--) {
        if (h_count >= MAX_CMDS) break;

        int exists = 0;
        for (int j = 0; j < h_count; j++) {
            if (strcmp(history[j].cmd, all_lines[i]) == 0) {
                exists = 1;
                break;
            }
        }
        if (!exists) {
            strncpy(history[h_count].cmd, all_lines[i], CMD_LEN);
            h_count++;
        }
    }

    if (h_count == 0) {
        printf("登録できる履歴がありません。\n");
        return;
    }

    int selected = 0;
    int top_view = (h_count > VIEW_WINDOW) ? VIEW_WINDOW - 1 : h_count - 1;

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    while (1) {
        clear_screen();
        printf("--- 登録する履歴を選択してください (addcmd) [%d件] ---\n", h_count);

        if (selected > top_view) {
            top_view = selected;
        }
        if (selected <= top_view - VIEW_WINDOW) {
            top_view = selected + VIEW_WINDOW - 1;
        }

        int end_idx = top_view;
        int start_idx = end_idx - VIEW_WINDOW + 1;
        if (start_idx < 0) start_idx = 0;
        if (end_idx >= h_count) end_idx = h_count - 1;

        for (int i = end_idx; i >= start_idx; i--) {
            int display_num = i + 1;

            if (i == selected) {
                printf(" > [%d] %s\n", display_num, history[i].cmd);
            } else {
                printf("   [%d] %s\n", display_num, history[i].cmd);
            }
        }
        printf("--------------------------------------------------\n");
        printf("(上矢印:古い履歴(上)へ遡る / 下矢印:新しい履歴(下)へ戻る / Enter:登録 / ESC:終了)\n");

        int key = 0;
        while ((key = get_linux_key()) == 0) {
            usleep(10000);
        }

        if (key == 27) break;

        if (key == 1001) {
            if (selected < h_count - 1) {
                selected++;
            }
        }
        else if (key == 1002) {
            if (selected > 0) {
                selected--;
            }
        }
        else if (key == 10 || key == 13) {
            char list_path[512];
            get_list_path(list_path, sizeof(list_path));
            FILE *df = fopen(list_path, "a");
            if (df) {
                fprintf(df, "%s\n", history[selected].cmd);
                fclose(df);
            }
            break;
        }
    }

    fcntl(STDIN_FILENO, F_SETFL, oldf);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

// ==========================================
// 2. selcmd (登録コマンドからの選択)
// ==========================================
void do_selcmd(int sort_abc) {
    char list_path[512];
    get_list_path(list_path, sizeof(list_path));

    CmdItem items[MAX_CMDS];
    int item_count = 0;

    FILE *fp = fopen(list_path, "r");
    if (!fp) {
        fp = fopen(list_path, "a+");
    }

    if (fp) {
        rewind(fp);
        while (item_count < MAX_CMDS && fgets(items[item_count].cmd, CMD_LEN, fp)) {
            items[item_count].cmd[strcspn(items[item_count].cmd, "\n")] = 0;
            if (strlen(items[item_count].cmd) > 0) item_count++;
        }
        fclose(fp);
    }

    if (item_count == 0) {
        printf("登録されたコマンドがありません。addcmd で登録してください。\n");
        return;
    }

    if (sort_abc) {
        qsort(items, item_count, sizeof(CmdItem), compare_abc);
    } else {
        for (int i = 0; i < item_count / 2; i++) {
            CmdItem temp = items[i];
            items[i] = items[item_count - 1 - i];
            items[item_count - 1 - i] = temp;
        }
    }

    char query[100] = {0};
    int q_len = 0;
    int selected = 0;
    int top_view = 0;

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    while (1) {
        clear_screen();
        printf("Query> %s\n", query);
        printf("--------------------------------------------------\n");

        int matched_indices[MAX_CMDS];
        int match_count = 0;

        for (int i = 0; i < item_count; i++) {
            if (q_len == 0) {
                matched_indices[match_count++] = i;
            } else if (query[0] == '*') {
                if (query[1] == '\0' || strstr(items[i].cmd, &query[1]) != NULL) {
                    matched_indices[match_count++] = i;
                }
            } else {
                if (strstr(items[i].cmd, query) == items[i].cmd) {
                    matched_indices[match_count++] = i;
                }
            }
        }

        if (match_count == 0) {
            printf("  (該当するコマンドがありません)\n");
        }

        if (selected >= match_count) selected = (match_count > 0) ? match_count - 1 : 0;
        if (selected < 0) selected = 0;

        if (selected < top_view) top_view = selected;
        if (selected >= top_view + VIEW_WINDOW) top_view = selected - VIEW_WINDOW + 1;
        int end_view = (top_view + VIEW_WINDOW > match_count) ? match_count : top_view + VIEW_WINDOW;

        for (int i = top_view; i < end_view; i++) {
            int idx = matched_indices[i];
            if (i == selected) {
                printf(" > [%d] %s\n", idx + 1, items[idx].cmd);
            } else {
                printf("   [%d] %s\n", idx + 1, items[idx].cmd);
            }
        }
        printf("--------------------------------------------------\n");
        printf("(上下矢印:選択 / 文字入力:絞り込み(頭*で部分一致) / Enter:確定 / Delete:削除 / ESC:終了)\n");

        int key = 0;
        while ((key = get_linux_key()) == 0) {
            usleep(10000);
        }

        if (key == 27) break;

        if (key == 1001 && match_count > 0) {
            if (selected > 0) selected--;
        }
        else if (key == 1002 && match_count > 0) {
            if (selected < match_count - 1) selected++;
        }
        else if (key == 1003 && match_count > 0) {
            int del_target_idx = matched_indices[selected];
            for (int d = del_target_idx; d < item_count - 1; d++) {
                items[d] = items[d + 1];
            }
            item_count--;
            save_all_items(items, item_count, sort_abc);
        }
        else if (key == 10 || key == 13) {
            if (match_count > 0) {
                int target_idx = matched_indices[selected];
                char bash_hist_path[512];
                // ★【タイポ修正箇所】綺麗になりました
                get_bash_history_path(bash_hist_path, sizeof(bash_hist_path));
                FILE *bf = fopen(bash_hist_path, "a");
                if (bf) {
                    fprintf(bf, "%s\n", items[target_idx].cmd);
                    fclose(bf);
                }
            }
            break;
        }
        else if (key == 127 || key == 8) {
            if (q_len > 0) {
                query[--q_len] = '\0';
                selected = 0;
                top_view = 0;
            }
        }
        else if (key >= 32 && key <= 126) {
            if (q_len < 99) {
                query[q_len++] = (char)key;
                query[q_len] = '\0';
                selected = 0;
                top_view = 0;
            }
        }
    }

    fcntl(STDIN_FILENO, F_SETFL, oldf);
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int main(int argc, char *argv[]) {
    int sort_abc = 0;
    int mode_add = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "add") == 0) mode_add = 1;
        if (strcmp(argv[i], "-a") == 0) sort_abc = 1;
    }

    if (mode_add) {
        do_addcmd();
    } else {
        do_selcmd(sort_abc);
    }

    return 0;
}
