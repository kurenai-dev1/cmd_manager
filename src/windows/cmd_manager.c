#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>   // Windows用のキー入力 (_getch)
#include <windows.h> // Windowsのシステム制御

#define MAX_CMDS 100
#define CMD_LEN 1024
#define VIEW_WINDOW 15

typedef struct {
    char cmd[CMD_LEN];
} CmdItem;

// 画面を綺麗にクリアするWindows専用関数（system("cls")の文字化けバグ回避用）
void clear_screen() {
    COORD coord = {0, 0};
    DWORD count;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hStdout, &csbi);
    FillConsoleOutputCharacter(hStdout, (TCHAR) ' ', csbi.dwSize.X * csbi.dwSize.Y, coord, &count);
    SetConsoleCursorPosition(hStdout, coord);
}

// 共通のデータファイル保存先 (C:\Users\ユーザー名\.my_commands_data)
void get_data_path(char *path, size_t size) {
    snprintf(path, size, "%s\\.my_commands_data", getenv("USERPROFILE"));
}

int compare_abc(const void *a, const void *b) {
    return strcmp(((CmdItem*)a)->cmd, ((CmdItem*)b)->cmd);
}

void save_all_items(CmdItem *items, int count) {
    char data_path[512];
    get_data_path(data_path, sizeof(data_path));
    FILE *fp = fopen(data_path, "w");
    if (fp) {
        for (int i = 0; i < count; i++) {
            fprintf(fp, "%s\n", items[i].cmd);
        }
        fclose(fp);
    }
}

// 登録処理 (addcmd) - Windows空行徹底排除版
void do_addcmd() {
    char temp_history_path[512];
    snprintf(temp_history_path, sizeof(temp_history_path), "%s\\AppData\\Local\\Temp\\cmd_hist.txt", getenv("USERPROFILE"));

    // ※ doskeyからの書き出しはバッチファイル（addcmd.bat）が行うため、ここではファイルを開くだけ
    #ifdef _WIN32
    SetConsoleOutputCP(932);
    SetConsoleCP(932);
    #endif

    FILE *fp = fopen(temp_history_path, "r");
    if (!fp) {
        printf("履歴一時ファイル（cmd_hist.txt）が開けませんでした。\n");
        return;
    }

    char history[MAX_CMDS][CMD_LEN];
    int h_count = 0;
    char line[CMD_LEN];

    while (fgets(line, sizeof(line), fp)) {
        // ★ 修正ポイント: 右側の改行コード (\r や \n) やスペースを徹底的に除去
        int len = (int)strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ' || line[len - 1] == '\t')) {
            line[--len] = '\0';
        }
        
        // 文字列が空になったら確実にスキップ
        if (len == 0) continue;
        
        // addcmd や selcmd 自身を含む行は除外
        if (strstr(line, "addcmd") || strstr(line, "selcmd")) continue;
        
        // 直前のコマンドと完全に一致する場合は重複としてスキップ
        if (h_count > 0 && strcmp(history[(h_count - 1) % MAX_CMDS], line) == 0) continue;

        strncpy(history[h_count % MAX_CMDS], line, CMD_LEN);
        h_count++;
    }
    fclose(fp);
    remove(temp_history_path); // 読み終わったら一時ファイルを削除

    int total_h = (h_count > MAX_CMDS) ? MAX_CMDS : h_count;
    if (total_h == 0) {
        printf("登録できるヒストリーがありません。(ファイル内が空、または除外対象のみです)\n");
        return;
    }

    int selected = 0;
    int top_view = 0;

    while (1) {
        clear_screen();
        printf("--- 登録する履歴を選択してください (addcmd) [%d件] ---\n", total_h);

        if (selected < top_view) top_view = selected;
        if (selected >= top_view + VIEW_WINDOW) top_view = selected - VIEW_WINDOW + 1;

        int end_view = (top_view + VIEW_WINDOW > total_h) ? total_h : top_view + VIEW_WINDOW;

        for (int i = end_view - 1; i >= top_view; i--) {
            int idx = (h_count - 1 - i + MAX_CMDS) % MAX_CMDS;
            if (i == selected) {
                printf(" > [%d] %s\n", i + 1, history[idx]);
            } else {
                printf("   [%d] %s\n", i + 1, history[idx]);
            }
        }
        printf("--------------------------------------------------\n");
        printf("(上下矢印:選択 / Enter:登録 / ESC:終了)\n");

        int ch = _getch();
        if (ch == 27) {
            printf("キャンセルしました。\n");
            return;
        }
        if (ch == 13) {
            char data_path[512];
            get_data_path(data_path, sizeof(data_path));
            FILE *df = fopen(data_path, "a");
            if (df) {
                int idx = (h_count - 1 - selected + MAX_CMDS) % MAX_CMDS;
                fprintf(df, "%s\n", history[idx]);
                fclose(df);
                printf("「%s」を共通リストに保存しました。\n", history[idx]);
            }
            return;
        }
        if (ch == 0 || ch == 224) {
            ch = _getch();
            if (ch == 72 && total_h > 0) { selected = (selected + 1) % total_h; } 
            if (ch == 80 && total_h > 0) { selected = (selected - 1 + total_h) % total_h; } 
        }
    }
}

// 選択・リアルタイム絞り込み処理 (selcmd) - 前方一致 / 部分一致 切り替え版
void do_selcmd(int sort_abc) {
    char data_path[512];
    get_data_path(data_path, sizeof(data_path));

    CmdItem items[MAX_CMDS];
    int item_count = 0;

    #ifdef _WIN32
    SetConsoleOutputCP(932);
    SetConsoleCP(932);
    #endif

    FILE *fp = fopen(data_path, "r");
    if (fp) {
        while (item_count < MAX_CMDS && fgets(items[item_count].cmd, CMD_LEN, fp)) {
            items[item_count].cmd[strcspn(items[item_count].cmd, "\r\n")] = 0;
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

    while (1) {
        clear_screen();
        printf("Query> %s\n", query);
        printf("--------------------------------------------------\n");

        int matched_indices[MAX_CMDS];
        int match_count = 0;

        for (int i = 0; i < item_count; i++) {
            if (q_len == 0) {
                // クエリが空なら全件ヒット
                matched_indices[match_count++] = i;
            } else if (query[0] == '*') {
                // ★ 頭が '*' なら【部分一致】（'*'自体は除いた2文字目以降で検索）
                if (query[1] == '\0' || strstr(items[i].cmd, &query[1]) != NULL) {
                    matched_indices[match_count++] = i;
                }
            } else {
                // ★ 通常は【前方一致】（最初に見つかった位置が先頭(== cmd)であること）
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

        for (int i = end_view - 1; i >= top_view; i--) {
            int idx = matched_indices[i];
            if (i == selected) {
                printf(" > [%d] %s\n", i + 1, items[idx].cmd);
            } else {
                printf("   [%d] %s\n", i + 1, items[idx].cmd);
            }
        }
        printf("--------------------------------------------------\n");
        printf("(上下矢印:選択 / 文字入力:絞り込み(頭*で部分一致) / Enter:確定 / Delete:削除 / ESC:終了)\n");

        int ch = _getch();

        if (ch == 0 || ch == 224) {
            ch = _getch();
            if (ch == 72 && match_count > 0) { selected = (selected + 1) % match_count; } 
            if (ch == 80 && match_count > 0) { selected = (selected - 1 + match_count) % match_count; } 
            
            if (ch == 83) { // Delete
                if (match_count > 0) {
                    int del_target_idx = matched_indices[selected];
                    for (int d = del_target_idx; d < item_count - 1; d++) {
                        items[d] = items[d + 1];
                    }
                    item_count--;
                    
                    if (!sort_abc) {
                        for (int i = 0; i < item_count / 2; i++) { CmdItem temp = items[i]; items[i] = items[item_count - 1 - i]; items[item_count - 1 - i] = temp; }
                        save_all_items(items, item_count);
                        for (int i = 0; i < item_count / 2; i++) { CmdItem temp = items[i]; items[i] = items[item_count - 1 - i]; items[item_count - 1 - i] = temp; }
                    } else {
                        save_all_items(items, item_count);
                    }
                }
            }
            continue; 
        }

        if (ch == 27) { return; }

        if (ch == 13) { 
            if (match_count > 0) {
                int target_idx = matched_indices[selected];
                char clip_cmd[CMD_LEN + 64];
                snprintf(clip_cmd, sizeof(clip_cmd), "echo | set /p=\"%s\" | clip", items[target_idx].cmd);
                system(clip_cmd);
            }
            return;
        }

        if (ch == 8) { // Backspace
            if (q_len > 0) {
                query[--q_len] = '\0';
                selected = 0;
                top_view = 0;
            }
            continue;
        }

        if (ch >= 32 && ch <= 126) {
            if (q_len < 99) {
                query[q_len++] = (char)ch;
                query[q_len] = '\0';
                selected = 0; 
                top_view = 0;
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "add") == 0) {
        do_addcmd();
    } else {
        int sort_abc = 0;
        if (argc > 1 && strcmp(argv[1], "-a") == 0) {
            sort_abc = 1;
        }
        do_selcmd(sort_abc);
    }
    return 0;
}
