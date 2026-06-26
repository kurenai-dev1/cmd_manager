#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>   // Windows用のキー入力 (_getch)
#include <windows.h> // Windowsのシステム制御

#pragma comment(lib, "user32.lib")

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

// 登録処理 (addcmd) - Windows完全重複排除＆初期15件表示版
void do_addcmd() {
    char temp_history_path[512];
    snprintf(temp_history_path, sizeof(temp_history_path), "%s\\AppData\\Local\\Temp\\cmd_hist.txt", getenv("USERPROFILE"));

    #ifdef _WIN32
    SetConsoleOutputCP(932);
    SetConsoleCP(932);
    #endif

    FILE *fp = fopen(temp_history_path, "r");
    if (!fp) {
        printf("履歴一時ファイル（cmd_hist.txt）が開けませんでした。\n");
        return;
    }

    // 1. 一時的にファイル全体の行をそのまま格納する領域（最大5000行）
    #define ALL_HIST_MAX 5000
    static char all_lines[ALL_HIST_MAX][CMD_LEN];
    int total_lines = 0;
    char line[CMD_LEN];

    while (fgets(line, sizeof(line), fp) && total_lines < ALL_HIST_MAX) {
        // 右側の改行コード (\r や \n) やスペースを徹底的に除去
        int len = (int)strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ' || line[len - 1] == '\t')) {
            line[--len] = '\0';
        }
        
        // 空行は確実にスキップ
        if (len == 0) continue;
        
        // ツール自体の起動コマンド（mycmd、および単体の addcmd / selcmd）を厳密に除外
        if (strncmp(line, "mycmd", 5) == 0 || strcmp(line, "addcmd") == 0 || strcmp(line, "selcmd") == 0) {
            continue;
        }
        
        // いったんすべての行をそのまま溜める
        strncpy(all_lines[total_lines], line, CMD_LEN);
        total_lines++;
    }
    fclose(fp);
    remove(temp_history_path); // 読み終わったら一時ファイルを削除

    // 2. 後ろ（最新）から前（過去）に向かって走査し、一意なものだけを200件まで抽出
    char history[MAX_CMDS][CMD_LEN];
    int h_count = 0;

    for (int i = total_lines - 1; i >= 0; i--) {
        if (h_count >= MAX_CMDS) break;

        int exists = 0;
        for (int j = 0; j < h_count; j++) {
            if (strcmp(history[j], all_lines[i]) == 0) {
                exists = 1; // 既に格納済みのより新しい同じコマンドを発見
                break;
            }
        }
        // まだ登録されていなければ採用
        if (!exists) {
            strncpy(history[h_count], all_lines[i], CMD_LEN);
            h_count++;
        }
    }

    int total_h = h_count; // 重複を削った最終的な一意の件数
    if (total_h == 0) {
        printf("登録できるヒストリーがありません。(ファイル内が空、または除外対象のみです)\n");
        return;
    }

    // 初期選択位置：配列の [0] （＝ 一番新しいコマンド ＝ 画面の最下部）
    int selected = 0;
    // 初期状態の表示ウィンドウ基準（最新の15件を表示）
    int top_view = (total_h > VIEW_WINDOW) ? VIEW_WINDOW - 1 : total_h - 1;

    // 3. インタラクティブ画面ループ
    while (1) {
        clear_screen();
        printf("--- 登録する履歴を選択してください (addcmd) [%d件] ---\n", total_h);

        // スクロール枠の連動計算
        if (selected > top_view) {
            top_view = selected;
        }
        if (selected <= top_view - VIEW_WINDOW) {
            top_view = selected + VIEW_WINDOW - 1;
        }

        int end_idx = top_view;               
        int start_idx = end_idx - VIEW_WINDOW + 1; 
        if (start_idx < 0) start_idx = 0;
        if (end_idx >= total_h) end_idx = total_h - 1;

        // 描画：上（古い＝インデックス大）から 下（新しい＝インデックス小＝[1]）へ
        for (int i = end_idx; i >= start_idx; i--) {
            int display_num = i + 1; 

            if (i == selected) {
                printf(" > [%d] %s\n", display_num, history[i]);
            } else {
                printf("   [%d] %s\n", display_num, history[i]);
            }
        }
        printf("--------------------------------------------------\n");
        printf("(上矢印:古い履歴(上)へ遡る / 下矢印:新しい履歴(下)へ戻る / Enter:登録 / ESC:終了)\n");

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
                fprintf(df, "%s\n", history[selected]);
                fclose(df);
                printf("「%s」を共通リストに保存しました。\n", history[selected]);
            }
            return;
        }
        if (ch == 0 || ch == 224) {
            ch = _getch();
            // 上矢印：古い方（画面の上＝配列のインデックスを増やす）
            if (ch == 72) { 
                if (selected < total_h - 1) selected++;
            } 
            // 下矢印：新しい方（画面の下＝配列のインデックスを減らす）
            if (ch == 80) { 
                if (selected > 0) selected--;
            } 
        }
    }
}

// Windows APIを使って安全に文字列をクリップボードにコピーする関数
void copy_to_clipboard(const char *text) {
    size_t len = strlen(text);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len + 1);
    if (!hMem) return;

    memcpy(GlobalLock(hMem), text, len + 1);
    GlobalUnlock(hMem);

    if (OpenClipboard(NULL)) {
        EmptyClipboard();
        SetClipboardData(CF_TEXT, hMem);
        CloseClipboard();
    } else {
        GlobalFree(hMem);
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

                // ★追加した関数を呼び出すだけ！超安全でシンプルに
                copy_to_clipboard(items[target_idx].cmd);

                // char clip_cmd[CMD_LEN + 64];
                // snprintf(clip_cmd, sizeof(clip_cmd), "echo | set /p=\"%s\" | clip", items[target_idx].cmd);
                // system(clip_cmd);
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
