#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <chrono>
#include <iomanip>
#include <array>
#include <mariadb/conncpp.hpp>

#define db_host "localhost"
#define db_user "ethereum"
#define db_pass "1234"
#define db_name "ethereum"

#define IS_WRITE(x) (x & 1)
#define IS_READ(x) (!(x & 1))
#define IS_ACTIVE(x) (!(x & 0x80000000))
#define IS_INACTIVE(x) (x & 0x80000000)

class state {
  public:
  int address_id;
  int blocknumber;
  int8_t type;
  state();
  state(int address_id, int blocknumber, int8_t type): address_id(address_id), blocknumber(blocknumber), type(type){};
};

//the restore list
std::map<int, std::vector<int> > restore;
//a map to cache the most recent appearance of an account
std::unordered_map<int, int> cache_account;
//a map to cache updated account in each block
std::map<int, std::set<int> > cache_block;

//address id - address map
std::unordered_map<int, std::array<uint8_t, 20> > addresses;
std::unordered_map<int, int > addresses_type;

int epoch_inactivate_every = 100000;
int epoch_inactivate_older_than = 100000;
int block_start = 0;
int block_end = 1000000;
std::string output_restore = "restore.json";
std::string output_address_type;
int log_period = 10000;

int cnt_restore = 0;
int cnt_inactivated = 0;

bool print_address_type = false;

//update account cache
void update_account (int address, int blocknumber, int type) {
  if (IS_READ(type)) {
    //restore inactive account on read
    if (cache_account.contains(address) && IS_INACTIVE(cache_account[address])) {
      restore[blocknumber].push_back(address);
      cnt_restore++;
      cache_account[address] = blocknumber;
    }
  } else if (IS_WRITE(type)) {
    //restore inactive account on write
    if (cache_account.contains(address) && IS_INACTIVE(cache_account[address])) {
      restore[blocknumber].push_back(address);
      cnt_restore++;
    }
    //update account cache
    cache_account[address] = blocknumber;
  }
}

int run(int32_t from, int32_t to) {
  const int batch_size = 100;
  const int batch_size_address = 10000;

  int cnt_state = 0, cnt_block = 0;
  int max_id = 0;

  sql::Driver* driver = sql::mariadb::get_driver_instance();
  sql::SQLString url(std::string("tcp://") + db_host + std::string("/") + std::string(db_name));
  sql::Properties properties({{"user", db_user}, {"password", db_pass}});
  std::unique_ptr<sql::Connection> conn(driver->connect(url, properties));

  sql::ResultSet *query;

  if (!conn) {
    std::cout<<"DB connection failed"<<std::endl;
    return 1;
  }

  auto start = std::chrono::steady_clock::now();

  for (int32_t i = from; i <= to; i += batch_size) {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("SELECT `address_id`, `blocknumber`, `type` FROM `states` WHERE `blocknumber`>=? AND `blocknumber`<?;"));
    stmnt->setInt(1, i);
    stmnt->setInt(2, std::min(i+batch_size, to+1));

    std::vector<std::vector<state> > result;

    result.reserve(batch_size);
    for (int32_t j = 0; j < batch_size; ++j) result.push_back(std::vector<state>());

    query = stmnt->executeQuery();

    while (query->next()) {
      result[query->getInt(2)-i].push_back(state(query->getInt(1), query->getInt(2), query->getInt(3)));
    }
    delete query;

    for (int32_t k = 0; k < batch_size; ++k) {
      cache_block[i+k] = std::set<int>();
      //update accounts
      for (auto const& j : result[k]) {
        try {
          if (j.address_id > max_id) max_id = j.address_id;
          if (cache_account.contains(j.address_id)) {
            if (IS_WRITE(j.type) && cache_account[j.address_id] >= i+k - epoch_inactivate_every - 1 - epoch_inactivate_older_than && IS_ACTIVE(cache_account[j.address_id])) {
              //erase an existing outdated cache
              cache_block[cache_account[j.address_id]].erase(j.address_id);
            }
          }
          update_account(j.address_id, i+k, j.type);
          //cache the potentially restorable account
          if (cache_account.contains(j.address_id)) cache_block[i+k].insert(j.address_id);
          cnt_state++;
        } catch (int err) {
          std::cout<<"Error blk #"<<j.blocknumber<<std::endl;
        }
      }

      //inactivate accounts
      if ((i+k + 1 - from + epoch_inactivate_older_than) % epoch_inactivate_every == 0) {
        //iterate over the blocks
        for (int32_t j = epoch_inactivate_every-1; j >= 0; --j) {
          int32_t block_removal = i+k - j - epoch_inactivate_older_than;
          if (block_removal >= 0) {
            //iterate over block cache to inactivate the accounts
            for (auto const& l : cache_block[block_removal]) {
              if (IS_ACTIVE(cache_account[l]) && cache_account[l] <= i+k - epoch_inactivate_older_than) {
                //mark as inactivated
                cache_account[l] = block_removal ^ 0x80000000;
                cnt_inactivated++;
              }
            }
            cache_block.erase(block_removal);
          }
        }
      }
      //print logs
      if ((i+k) % log_period == 0) {
        int ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
        std::cout<<"========================================"<<std::endl;
        std::cout<<"Blk height: "<<i+k<<std::endl;
        std::cout<<"  Blkn: "<<cnt_block<<"("<<std::fixed<<std::setprecision(2)<<(cnt_block*1000.0/ms)<<"/s), Staten: "<<cnt_state<<"("<<std::fixed<<std::setprecision(2)<<(cnt_state*1000.0/ms)<<"/s) in "<<ms<<"ms"<<std::endl;
        std::cout<<"  Inactivated "<<cnt_inactivated<<"accs, Restored: "<<cnt_restore<<"accs, Unique: "<<cache_account.size()<<"accs"<<std::endl;
      }
    }
    cnt_block = std::min(cnt_block+batch_size, to);
  }

  //for GC
  std::map<int, std::set<int> >().swap(cache_block);

  int ms;
  ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  std::cout<<"Processing restore list ("<<ms<<"ms)"<<std::endl;

  std::unique_ptr<sql::Statement> stmnt(conn->createStatement());
  query = stmnt->executeQuery("SELECT MAX(`id`) FROM `addresses`;");
  query->next();
  int address_cnt = query->getInt(1);
  address_cnt = std::min(address_cnt, max_id);
  delete query;

  char tmp[20];

  ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  std::cout<<"Fetching addresses ("<<ms<<"ms)"<<std::endl;

  //fetch address informations
  for (int i = 0; i < address_cnt; i+=batch_size_address) {
    std::unique_ptr<sql::PreparedStatement> stmnt(conn->prepareStatement("SELECT `id`, `address`, `_type` FROM `addresses` WHERE `id`>=? AND `id`<?;"));
    stmnt->setInt(1, i);
    stmnt->setInt(2, i+batch_size_address);
    sql::ResultSet *query = stmnt->executeQuery();
    while (query->next()) {
      std::istream *address = query->getBlob(2);
      address->read(tmp, 20);
      delete address;
      std::copy(tmp, tmp+20, std::begin(addresses[query->getInt(1)]));
      if (print_address_type) {
        addresses_type[query->getInt(1)] = query->getInt(3);
      }
    }
    delete query;
  }

  std::ofstream output_file, output_file_address_type;
  output_file.open(output_restore);
  output_file<<"{"<<std::endl;

  //write output
  if (print_address_type) {
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    std::cout<<"Writing type output ("<<ms<<"ms)"<<std::endl;
    output_file_address_type.open(output_address_type);
    output_file_address_type<<"{"<<std::endl;
    output_file_address_type<<"  \"inactive\": {"<<std::endl;
    int first_block = true;
    for (auto const& i : cache_account) {
      if (i.second & 0x80000000) {
        if (first_block == true) {
          first_block = false;
        } else {
          output_file_address_type<<","<<std::endl;
        }
        output_file_address_type<<"    \"0x";
        for (int k = 0; k < 20; ++k) {
          output_file_address_type<<std::hex<<std::setfill('0')<<std::setw(2)<<(int)addresses[i.first][k];
        }
        output_file_address_type<<"\": {\"last\": "<<std::dec<<(i.second^0x80000000)<<", \"type\": "<<addresses_type[i.first]<<"}";
      }
    }
    output_file_address_type<<std::endl<<"  },"<<std::endl;
    output_file_address_type<<"  \"active\": {"<<std::endl;
    first_block = true;
    for (auto const& i : cache_account) {
      if (!(i.second & 0x80000000)) {
        if (first_block == true) {
          first_block = false;
        } else {
          output_file_address_type<<","<<std::endl;
        }
        output_file_address_type<<"    \"0x";
        for (int k = 0; k < 20; ++k) {
          output_file_address_type<<std::hex<<std::setfill('0')<<std::setw(2)<<(int)addresses[i.first][k];
        }
        output_file_address_type<<"\": {\"last\": "<<std::dec<<i.second<<", \"type\": "<<addresses_type[i.first]<<"}";
      }
    }
    output_file_address_type<<std::endl<<"  }"<<std::endl;
    output_file_address_type<<"}"<<std::endl;
    output_file_address_type.close();
    ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    std::cout<<"Saved accounts as "<<output_address_type<<" ("<<ms<<"ms)"<<std::endl;
  }
  cache_account.clear();

  ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  std::cout<<"Writing restorelist output ("<<ms<<"ms)"<<std::endl;

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

  ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
  std::cout<<"Saved result as "<<output_restore<<" ("<<ms<<"ms)"<<std::endl;

  return 0;
}

int main(int argc, char **argv) {
  int opt;
  while ((opt = getopt(argc, argv, "s:e:i:t:o:l:y:")) != -1) {
    switch ( opt ) {
      case 's':
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
      case 'y':
        print_address_type = true;
        output_address_type = std::string(optarg);
        break;
      case '?':
        break;
    }
  }
  
  return run(block_start, block_end);
}