# cmd_manager
プロンプト画面で良く使うコマンドを簡単登録・検索

## 概要(Overview) 

Windows/linux のコンソール画面で良く使うコマンドを登録。　
検索してカーソルで呼び出せるコマンドです。  
本プログラムは、Google Gemini に全て作らせました。

## デモ画面(Demo)


## 導入方法(Setup)

WindowsとLinux(bash)の２つの環境に対応します。  
Windows は Visual Studio、 Linux は gcc でコンパイルして下さい。 

### Windows
ソースファイルとバッチファイルは、UTF-8 では無く、Shift-JIS(ANSI) で保存します。  
Visual Studio でコンパイルを行う。  
cl cmd_manager.c  
出来た exe ファイルを PATH の通るディレクトリに置きます。　
さらに、addcmd.bat selcmd.bat も作成します。(PATHは置いた場所に修正)　

### Linux(bash)
  
gcc でコンパイルを行う。
gcc cmd_manager.c  
実行できるようにコピーする　
cp cmd_manager /usr/local/bin　
setting.sh の例のように、~/.bashrc に定義する。
source ~/.bashrc

## 操作方法(Usage)

addcmd で履歴が表示されるので追加したいコマンドをカーソルキーで選択する。  
selcmd で登録したコマンドが表示されるのでカーソルで選択する。
文字を入力すると前方一致でコマンドを絞り込めます。(*で部分一致)
選択したコマンドは以下の方法で呼び出せます。  

Windows：クリップボードからの貼り付け(Ctrl+V)    
Linux：コマンド履歴に登録される。 

## 特徴

Linux では、fzf 等が有名ですが、そこまでの機能は必要無いと思う人向け。　
Windows ではそもそも履歴がコンソールを開いている間しか残らないので、コンソール間で再利用できません。  
ユーザー単位に共有できるので劇的に改善されます。　
