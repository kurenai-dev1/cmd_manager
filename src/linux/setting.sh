alias addcmd='cmd_manager add'

function selcmd() {
    # 1. 現在のメモリ上の履歴をすべてファイルに書き出し、完全に同期させる
    history -w

    # 2. C言語を実行（ファイル末尾に選んだ1件だけが綺麗に追記される）
    /usr/local/bin/cmd_manager "$@"

    # 3. Bashが持っている「不整合を起こしたメモリ上の履歴」を一度完全にクリア
    history -c

    # 4. C言語が書き換えた最新のファイルを、まっさらなメモリに正しくロード
    history -r
}
