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
* Add location of MariaDB library to environment variable ```LD_LIBRARY_PATH```.
```
$ export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/lib64
```