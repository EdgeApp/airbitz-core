/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef UTIL_READ_LINE_HPP
#define UTIL_READ_LINE_HPP

#include <string>
#include <thread>
#include <zmq.hpp>

/*
 * Reads lines from the terminal in a separate thread.
 *
 * A networking thread cannot use the C++ standard library to read from the
 * terminal. Once the thread calls std::cin.getline or similar, it becomes
 * stuck until the user types something, so the thread cannot handle network
 * events at the same time. Therefore, the network stuff and the terminal
 * stuff need to run in separate threads.
 *
 * The simplest solution is to create a thread that simply reads from the
 * terminal and transmits the results over a zeromq inproc socket. The main
 * thread sends an empty REQ message when it wants to read from the terminal,
 * and the reader thread sends back a REP message with whatever the user
 * typed. If the main thread sends a non-empty REQ message, the thread quits.
 *
 * To use this class, first call `show_prompt`. This call will display a
 * command prompt and begin reading input in the background. Then, use
 * `pollitem` with `zmq_poll` to determine when the line is available. Once
 * the line is available, use `get_line` to retrieve it.
 *
 * If you attempt to destroy this class while reading a line, the destructor
 * will block until the user finishes their entry.
 */
class ReadLine
{
public:
    ~ReadLine();
    ReadLine(zmq::context_t &context);

    /**
     * Displays a command prompt and begins reading a line in the background.
     */
    void show_prompt();

    /**
     * Creates a zeromq pollitem_t structure suitable for passing to the
     * zmq_poll function. The zmq_poll function will indicate that there is
     * data waiting to be read once a line is available.
     */
    zmq_pollitem_t pollitem();

    /**
     * Retrieves the line requested by `show_prompt`. This method will
     * return a blank string if no line is available yet.
     */
    std::string get_line();

private:
    void run(zmq::context_t *context);

    zmq::socket_t socket_;
    std::thread *thread_;
};

#endif
