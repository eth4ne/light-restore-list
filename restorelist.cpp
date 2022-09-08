#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <chrono>
#include <iomanip>
#include <array>
#include <mariadb/conncpp.hpp>

#define db_host "localhost"
#define db_user "ethereum"
#define db_pass "1234"
#define db_name "ethereum"

class state {
  public:
  int address_id;
  int blocknumber;
  int8_t type;
  state();
  state(int address_id, int blocknumber, int8_t type): address_id(address_id), blocknumber(blocknumber), type(type){};
};

std::map< int, std::vector< int > > restore;
std::map< int, int > cache_account;
std::map< int, std::vector< int > > cache_block;

std::map< int, std::array< uint8_t, 20 > > addresses;

int epoch_inactivate_every = 100000;
int epoch_inactivate_older_than = 100000;
int block_start = 0;
int block_end = 1000000;
std::string output_restore = "restore.json";
int log_period = 10000;

int cnt_restore = 0;
int cnt_inactivated = 0;

void update_account (int address, int blocknumber, int type) {
  if (type == 0) {
    if (cache_account.contains(address) && cache_account[address] < 0) {
      restore[blocknumber].push_back(address);
      cnt_restore++;
      cache_account[address] = blocknumber;
    }
  } else if (type == 1) {
    if (cache_account.contains(address) && cache_account[address] < 0) {
      restore[blocknumber].push_back(address);
      cnt_restore++;
    }
    cache_account[address] = blocknumber;
  }
}

int run(int from, int to) {
  const int batch_size = 100;
  const int batch_size_address = 10000;

  int cnt_state = 0, cnt_block = 0;
  int max_id = 0;

  sql::Driver* driver = sql::mariadb::get_driver_instance();
  sql::SQLString url(std::string("tcp://") + db_host + std::string("/") + std::string(db_name));
  sql::Properties properties({{"user", db_user}, {"password", db_pass}});
  std::unique_ptr<sql::Connection> conn(driver->connect(url, properties));

  if (!conn) {
    std::cout<<"DB connection failed"<<std::endl;
    return 1;
  }

  auto start = std::chrono::steady_clock::now();

  for (int i = from; i <= to; i += batch_size) {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("SELECT `address_id`, `blocknumber`, `type` FROM `states` WHERE `blocknumber`>=? AND `blocknumber`<?;"));
    stmnt->setInt(1, i);
    stmnt->setInt(2, std::min(i+batch_size, to+1));

    std::vector< std::vector<state> > result;
    //std::fill(result.begin(), result.end(), std::vector<state>());
    for (int j = 0; j < batch_size; ++j) result.push_back(std::vector<state>());

    sql::ResultSet *query = stmnt->executeQuery();
    while (query->next()) {
      result[query->getInt(2)-i].push_back(state(query->getInt(1), query->getInt(2), query->getInt(3)));
    }
    delete query;

    for (int k = 0; k < batch_size; ++k) {
      cache_block[i+k] = std::vector< int > ();
      for (auto const& j : result[k]) {
        try {
          if (j.address_id > max_id) max_id = j.address_id;
          if (j.type % 2 == 0) {
            update_account(j.address_id, i+k, 0);
          } else {
            update_account(j.address_id, i+k, 1);
          }
          cache_block[i+k].push_back(j.address_id);
          cnt_state++;
        } catch (int err) {
          std::cout<<"Error blk #"<<j.blocknumber<<std::endl;
        }
      }

      //cache_block[i+k] = cache_block_tmp;

      if ((i+k + 1 - from) % epoch_inactivate_every == 0) {
        for (int j = 0; j < epoch_inactivate_every; ++j) {
          int block_removal = i+k - j - epoch_inactivate_older_than;
          if (cache_block.contains(block_removal)) {
            for (auto const& l : cache_block[block_removal]) {
              if (cache_account[l] == block_removal) {
                cache_account[l] = -block_removal;
                cnt_inactivated++;
              }
            }
            cache_block.erase(block_removal);
          }
        }
      }
      if ((i+k) % log_period == 0) {
        int ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::cout<<"Blk height: "<<i+k<<std::endl;
        std::cout<<" Blkn: "<<cnt_block<<"("<<std::fixed<<std::setprecision(2)<<(cnt_block*1000.0/ms)<<"/s), Staten: "<<cnt_state<<"("<<std::fixed<<std::setprecision(2)<<(cnt_state*1000.0/ms)<<"/s) in "<<ms<<"ms"<<std::endl;
        std::cout<<" Total restore: "<<cnt_restore<<"accs, total inactivated: "<<cnt_inactivated<<"accs"<<std::endl;
      }
    }
    cnt_block = std::min(cnt_block+batch_size, to);
  }

  cache_account.clear();
  std::cout<<"Processing restore list"<<std::endl;

  std::unique_ptr<sql::Statement> stmnt(conn->createStatement());
  sql::ResultSet *query = stmnt->executeQuery("SELECT COUNT(*) FROM `addresses`;");
  query->next();
  int address_cnt = query->getInt(1);
  address_cnt = std::min(address_cnt, max_id);
  delete query;

  char tmp[20];

  for (int i = 0; i < address_cnt; i+=batch_size_address) {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("SELECT `id`, `address` FROM `addresses` WHERE `id`>=? AND `id`<?;"));
    stmnt->setInt(1, i);
    stmnt->setInt(2, i+batch_size_address);
    sql::ResultSet *query = stmnt->executeQuery();
    while (query->next()) {
      std::istream *address = query->getBlob(2);
      address->read(tmp, 20);
      std::copy(tmp, tmp+20, std::begin(addresses[query->getInt(1)]));
    }
    delete query;
  }

  std::ofstream output_file;
  output_file.open(output_restore);
  output_file<<"{"<<std::endl;
  int first_block = true;
  for (auto const& i : restore) {
    if (first_block == true) {
      first_block = false;
    } else {
      output_file<<","<<std::endl;
    }
    output_file<<"  "<<"\""<<std::dec<<i.first<<"\": ["<<std::endl;
    int first_addr = true;
    for (auto const& j : i.second) {
      if (first_addr == true) {
        first_addr = false;
      } else {
        output_file<<","<<std::endl;
      }
      output_file<<"    \"0x";
      for (int k = 0; k < 20; ++k) {
        output_file<<std::hex<<std::setfill('0')<<std::setw(2)<<(int)addresses[j][k];
      }
      output_file<<"\"";
    }
    output_file<<std::endl<<"  "<<"]";
  }
  output_file<<std::endl<<"}";
  conn->close();
  output_file.close();


  std::cout<<"Saved result as "<<output_restore<<std::endl;
  return 0;
}

int main(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "s:e:i:t:o:l:")) != -1) {
    switch ( opt ) {
      case 's':
        std::cout<<optarg<<std::endl;
        block_start = std::stoi(std::string(optarg));
        break;
      case 'e':
        block_end = std::stoi(std::string(optarg));
        break;
      case 'i':
        epoch_inactivate_every = std::stoi(std::string(optarg));
        break;
      case 't':
        epoch_inactivate_older_than = std::stoi(std::string(optarg));
        break;
      case 'o':
        output_restore = std::string(optarg);
        break;
      case 'l':
        log_period = std::stoi(std::string(optarg));
        break;
      case '?':
        break;
    }
  }
  
  return run(block_start, block_end);
}