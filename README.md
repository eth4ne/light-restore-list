## Light restore list

### Prerequisites
* Install dependencies
```
$ sudo apt install build-essentials
```
* Install MariaDB C connector.
```
$ sudo apt install libmariadb3 libmariadb-dev
```
* Install MariaDB C++ connector. You may downloada a [prebuilt binary](https://mariadb.com/downloads/connectors/connectors-data-access/cpp-connector), or [build yourself.](https://github.com/mariadb-corporation/mariadb-connector-cpp/blob/master/BUILD.md)
* Install MariaDB library.
```
$ sudo install libmariadbcpp.so /usr/lib64
```
* Add location of MariaDB library to the environment variable ```LD_LIBRARY_PATH```.
```
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