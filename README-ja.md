[README in English is here](https://github.com/simotin13/CodeCoverage/blob/main/README.md)

# これは何？
このレポジトリは Intelにより公開されているDBI(Dynamic Binary Instrumentation)エンジンである[Pin](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html)を使ったコードカバレッジツールです。
Pinのプラグインとして動作します。

# サポートしているプラットフォーム
- GNU/Linux

# 使い方(Quick Start)
本レポジトリをダウンロードしてください。
```
git clone git@github.com:simotin13/CodeCoverage.git
```

実行にはPin本体が必要になります。
Pin本体は[Intel Pin](https://www.intel.com/content/www/us/en/developer/articles/tool/pin-a-dynamic-binary-instrumentation-tool.html)のサイトからダウンロードする必要があります。

ダウンロード＆ビルド、および実行例のスクリプトがこのレポジトリに含まれています。

まずは、`00_setup.sh`を実行してください。
```
cd CodeCoverage/
./00_setup.sh
```

`00_setup.sh` では、
[Pin 3.27](https://software.intel.com/sites/landingpage/pintool/downloads/pin-3.27-98718-gbeaa5d51e-gcc-linux.tar.gz)
をこのレポジトリと同じ階層にダウンロード・解凍しています。また、このレポジトリのソースコードをビルドします。

最新版のpinを使用する場合は、必要に応じてサイトからダウンロードしてください。

## カバレッジツールの実行
ダウンロードとビルドが完了したら、`01_run_example.sh` を実行してください。

`01_run_example.sh` ではこのコードカバレッジツールの実行例としてexamples/c_function_callに含まれるC言語のプログラムをカバレッジ計測対象として実行しています。

実行後 `report` フォルダにカバレッジの計測結果がHTMLファイルで出力されます。

# ビルドと実行コマンド
## ビルド
このツールをビルドする場合は
```
make PIN_ROOT=../pin-3.27-98718-gbeaa5d51e-gcc-linux
```
を実行してください。

pinのツールをビルドする際の作法として、PIN本体のディレクトリパスをPIN_ROOTで指定する必要があります。

## 実行
ツールを実行する場合は
```
../pin-3.27-98718-gbeaa5d51e-gcc-linux/pin -t ./obj-intel64/CodeCoverage.so -- <target_module_path> <target_args...>
```
のようにコマンドを実行してください。
