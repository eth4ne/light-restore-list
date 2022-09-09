## Light restore list

### Prerequisites
* Install dependencies
```
$ sudo apt install build-essentials libmariadb3 libmariadb-dev
```
* [Install MariaDB C++ connector](https://github.com/mariadb-corporation/mariadb-connector-cpp/blob/master/BUILD.md)
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
* <pre>-s <i>N</i></pre>: Block height to start with (inclusive) (default: 0)
* <pre>-e <i>N</i></pre>: Block height to end (inclusive) (default: 1000000)
* <pre>-i <i>N</i></pre>: Run inactivation every N blocks (default: 100000)
* <pre>-t <i>N</i></pre>: Inactivate addresses older than N blocks (default: 100000)
* <pre>-o <i>filename</i></pre>: Set output file name (default: restore.json)
* <pre>-l <i>N</i></pre>: Print log every N blocks (default: 10000)