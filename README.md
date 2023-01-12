## Light restore list

### Prerequisites
* Install dependencies
```Shell
$ sudo apt install build-essentials
```
* Install MariaDB C connector.
```Shell
$ sudo apt install libmariadb3 libmariadb-dev
```
* Install MariaDB C++ connector. You may download a [prebuilt binary](https://mariadb.com/downloads/connectors/connectors-data-access/cpp-connector) and extract it, or [build yourself.](https://github.com/mariadb-corporation/mariadb-connector-cpp/blob/master/BUILD.md)
  * To install prebuilt binary
```Shell
$ tar -xvzf mariadb-connector-cpp-*.tar.gz
$ cd mariadb-connector-cpp-*/
$ sudo install -d /usr/include/mariadb/conncpp
$ sudo install -d /usr/include/mariadb/conncpp/compat
$ sudo install include/mariadb/* /usr/include/mariadb/
$ sudo install include/mariadb/conncpp/* /usr/include/mariadb/conncpp
$ sudo install include/mariadb/conncpp/compat/* /usr/include/mariadb/conncpp/compat
```
* Install MariaDB library.
```Shell
$ sudo install libmariadbcpp.so /usr/lib64
```
* Make sure the location of MariaDB library is added to the environment variable ```$LD_LIBRARY_PATH```. The command below may be executed every time when logging in, or added to your shell profile(```~/.profile```).
```Shell
$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib64
```

### Building
* Build the executable.
```
$ make restorelist
```

### Command line options
* <code>-s <i>N</i></code>: Block height to start with (inclusive) (default: 0)
* <code>-e <i>N</i></code>: Block height to end (inclusive) (default: 1000000)
* <code>-i <i>N</i></code>: Run inactivation every N blocks (default: 100000)
* <code>-t <i>N</i></code>: Inactivate addresses older than N blocks (default: 100000)
* <code>-o <i>filename</i></code>: Set output file name (default: restore.json)
* <code>-l <i>N</i></code>: Print log every N blocks (default: 10000)
