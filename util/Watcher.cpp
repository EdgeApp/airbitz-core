/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */

#include "ReadLine.hpp"
#include "../abcd/bitcoin/TxUpdater.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <zmq.hpp>

using std::placeholders::_1;
using std::placeholders::_2;

/**
 * Command-line interface to the wallet watcher service.
 */
class Cli:
    public abcd::TxCallbacks
{
public:
    Cli();

    int run();

private:
    void command();
    void cmd_exit();
    void cmd_help();
    void cmd_connect(std::stringstream &args);
    void cmd_disconnect(std::stringstream &args);
    void cmd_watch(std::stringstream &args);
    void cmd_height();
    void cmd_tx_height(std::stringstream &args);
    void cmd_tx_dump(std::stringstream &args);
    void cmd_tx_send(std::stringstream &args);
    void cmd_utxos(std::stringstream &args);
    void cmd_save(std::stringstream &args);
    void cmd_load(std::stringstream &args);
    void cmd_dump(std::stringstream &args);

    // tx_callbacks interface:
    virtual void on_add(const bc::transaction_type &tx) override;
    virtual void on_height(size_t height) override;
    virtual void on_quiet() override;

    // Argument loading:
    bool read_string(std::stringstream &args, std::string &out,
                     const std::string &error_message);
    bc::hash_digest read_txid(std::stringstream &args);
    bool read_address(std::stringstream &args, bc::payment_address &out);

    // Networking:
    zmq::context_t context_;
    ReadLine terminal_;

    // State:
    abcd::TxDatabase db_;
    abcd::TxUpdater updater_;
    bool done_;
};

Cli::Cli():
    terminal_(context_),
    updater_(db_, context_, *this),
    done_(false)
{
}

/**
 * The main loop for the example application. This loop can be woken up
 * by either events from the network or by input from the terminal.
 */
int Cli::run()
{
    std::cout << "type \"help\" for instructions" << std::endl;
    terminal_.show_prompt();

    while (!done_)
    {
        std::vector<zmq_pollitem_t> items;
        items.push_back(terminal_.pollitem());
        auto updaterItems = updater_.pollitems();
        items.insert(items.end(), updaterItems.begin(), updaterItems.end());

        auto nextWakeup = updater_.wakeup();
        int delay = nextWakeup.count() ? nextWakeup.count() : -1;

        zmq::poll(items.data(), items.size(), delay);

        if (items[0].revents)
            command();
    }
    return 0;
}

/**
 * Reads a command from the terminal thread, and processes it appropriately.
 */
void Cli::command()
{
    std::stringstream reader(terminal_.get_line());
    std::string command;
    reader >> command;

    if (command == "")                  ;
    else if (command == "exit")         cmd_exit();
    else if (command == "help")         cmd_help();
    else if (command == "connect")      cmd_connect(reader);
    else if (command == "disconnect")   cmd_disconnect(reader);
    else if (command == "height")       cmd_height();
    else if (command == "watch")        cmd_watch(reader);
    else if (command == "txheight")     cmd_tx_height(reader);
    else if (command == "txdump")       cmd_tx_dump(reader);
    else if (command == "txsend")       cmd_tx_send(reader);
    else if (command == "utxos")        cmd_utxos(reader);
    else if (command == "save")         cmd_save(reader);
    else if (command == "load")         cmd_load(reader);
    else if (command == "dump")         cmd_dump(reader);
    else
        std::cout << "unknown command " << command << std::endl;

    // Display another prompt, if needed:
    if (!done_)
        terminal_.show_prompt();
}

void Cli::cmd_exit()
{
    done_ = true;
}

void Cli::cmd_help()
{
    std::cout << "commands:" << std::endl;
    std::cout << "  exit                - leave the program" << std::endl;
    std::cout << "  help                - this menu" << std::endl;
    std::cout << "  connect <server>    - connect to obelisk server" << std::endl;
    std::cout << "  disconnect          - stop talking to the obelisk server" <<
              std::endl;
    std::cout << "  height              - get the current blockchain height" <<
              std::endl;
    std::cout << "  watch <address> [poll ms] - watch an address" << std::endl;
    std::cout << "  txheight <hash>     - get a transaction's height" << std::endl;
    std::cout << "  txdump <hash>       - show the contents of a transaction" <<
              std::endl;
    std::cout << "  txsend <hash>       - push a transaction to the server" <<
              std::endl;
    std::cout << "  utxos [address]     - get utxos for an address" << std::endl;
    std::cout << "  save <filename>     - dump the database to disk" << std::endl;
    std::cout << "  load <filename>     - load the database from disk" << std::endl;
    std::cout << "  dump [filename]     - display the database contents" <<
              std::endl;
}

void Cli::cmd_connect(std::stringstream &args)
{
    updater_.connect().log();
}

void Cli::cmd_disconnect(std::stringstream &args)
{
    updater_.disconnect();
}

void Cli::cmd_height()
{
    std::cout << db_.last_height() << std::endl;
}

void Cli::cmd_tx_height(std::stringstream &args)
{
    bc::hash_digest txid = read_txid(args);
    if (txid == bc::null_hash)
        return;

    std::cout << db_.txidHeight(txid) << std::endl;
}

void Cli::cmd_tx_dump(std::stringstream &args)
{
    bc::hash_digest txid = read_txid(args);
    if (txid == bc::null_hash)
        return;
    bc::transaction_type tx = db_.txidLookup(txid);

    std::basic_ostringstream<uint8_t> stream;
    auto serial = bc::make_serializer(std::ostreambuf_iterator<uint8_t>(stream));
    serial.set_iterator(satoshi_save(tx, serial.iterator()));
    auto str = stream.str();
    std::cout << bc::encode_hex(str) << std::endl;
}

void Cli::cmd_tx_send(std::stringstream &args)
{
    std::string arg;
    args >> arg;
    bc::data_chunk data = bc::decode_hex(arg);
    bc::transaction_type tx;
    try
    {
        bc::satoshi_load(data.begin(), data.end(), tx);
    }
    catch (bc::end_of_stream)
    {
        std::cout << "not a valid transaction" << std::endl;
        return;
    }
    updater_.send(tx);
}

void Cli::cmd_watch(std::stringstream &args)
{
    bc::payment_address address;
    if (!read_address(args, address))
        return;
    unsigned poll_ms = 10000;
    args >> poll_ms;
    if (poll_ms < 500)
    {
        std::cout << "warning: poll too short, setting to 500ms" << std::endl;
        poll_ms = 500;
    }
    updater_.watch(address, bc::client::sleep_time(poll_ms));
}

void Cli::cmd_utxos(std::stringstream &args)
{
    bc::output_info_list utxos;
    utxos = db_.get_utxos(updater_.watching());

    // Display the output:
    size_t total = 0;
    for (auto &utxo: utxos)
    {
        std::cout << bc::encode_hash(utxo.point.hash) << ":" <<
                  utxo.point.index << std::endl;
        auto tx = db_.txidLookup(utxo.point.hash);
        auto &output = tx.outputs[utxo.point.index];
        bc::payment_address to_address;
        if (bc::extract(to_address, output.script))
            std::cout << "address: " << to_address.encoded() << " ";
        std::cout << "value: " << output.value << std::endl;
        total += output.value;
    }
    std::cout << "total: " << total << std::endl;
}

void Cli::cmd_save(std::stringstream &args)
{
    std::string filename;
    if (!read_string(args, filename, "no filename given"))
        return;

    std::ofstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "cannot open " << filename << std::endl;
        return;
    }

    auto db = db_.serialize();
    file.write(reinterpret_cast<const char *>(db.data()), db.size());
    file.close();
}

void Cli::cmd_load(std::stringstream &args)
{
    std::string filename;
    if (!read_string(args, filename, "no filename given"))
        return;

    std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        std::cerr << "cannot open " << filename << std::endl;
        return;
    }

    std::streampos size = file.tellg();
    uint8_t *data = new uint8_t[size];
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(data), size);
    file.close();

    if (!db_.load(bc::data_chunk(data, data + size)))
        std::cerr << "error while loading data" << std::endl;
}

void Cli::cmd_dump(std::stringstream &args)
{
    std::string filename;
    args >> filename;
    if (filename.size())
    {
        std::ofstream file(filename);
        if (!file.is_open())
        {
            std::cerr << "cannot open " << filename << std::endl;
            return;
        }
        db_.dump(file);
    }
    else
        db_.dump(std::cout);
}

void Cli::on_add(const libbitcoin::transaction_type &tx)
{
    auto txid = libbitcoin::encode_hash(libbitcoin::hash_transaction(tx));
    std::cout << "got transaction " << txid << std::endl;
}

void Cli::on_height(size_t height)
{
    std::cout << "got block " << height << std::endl;
}

void Cli::on_quiet()
{
    std::cout << "query done" << std::endl;
    std::cout << "> " << std::flush;
}

/**
 * Parses a string argument out of the command line,
 * or prints an error message if there is none.
 */
bool Cli::read_string(std::stringstream &args, std::string &out,
                      const std::string &error_message)
{
    args >> out;
    if (!out.size())
    {
        std::cout << error_message << std::endl;
        return false;
    }
    return true;
}

bc::hash_digest Cli::read_txid(std::stringstream &args)
{
    std::string arg;
    args >> arg;
    if (!arg.size())
    {
        std::cout << "no txid given" << std::endl;
        return bc::null_hash;
    }
    std::hash_digest out;
    if (!bc::decode_hash(out, arg))
    {
        std::cout << "bad txid" << std::endl;
        return bc::null_hash;
    }
    return out;
}

/**
 * Reads a bitcoin address from the command-line, or prints an error if
 * the address is missing or invalid.
 */
bool Cli::read_address(std::stringstream &args, bc::payment_address &out)
{
    std::string address;
    if (!read_string(args, address, "error: no address given"))
        return false;
    if (!out.set_encoded(address))
    {
        std::cout << "error: invalid address " << address << std::endl;
        return false;
    }
    return true;
}

int main()
{
    Cli c;
    return c.run();
}
