/*
 * New and evolving utility to send Trace messages over the network
 * to a server that can display them.
 *
 * When the dust settles, factor out some of the common network handling
 * into a Network utility class.
 *
 * When a new connection fails:
 *
 * The most likely reason is that the server window is not running.
 * There are a few options to explore here.
 *
 * Easiest is just keep retrying every time we try to trace, and hope
 * it eventually succeeds.  This will however result in repeated error message
 * in the raw trace log.
 *
 * Could suppress the error logging for a period of time, say every 10 attempts,
 * but really if you've bothered to enable this, the other side shoudl be working ASAP.
 *
 * The nicest thing to do would be to try to automatically detect when the
 * debug window is available.  This requires a background thread to poll it.
 * Only until that succeeds will we attempt to use it.
 *
 * Another option is to have the debug window contact US when it goes up or down
 * and do not attempt to send to it unless we've been contacted.  That requires
 * a connection listener thread and is kind of a backward way of doing things.
 * Not seeing any big advantages over polling.
 *
 * First problem:
 *
 * socket->connect hangs for the given timeout waiting for the server to respond
 * If you forgot to bring up the server, or just don't want to use it we can't
 * be doing this every time a trace message wants to emit.  So do it once and
 * give up after the first failure.  Or maybe try it again after 10 times,
 * or wait to be told by the UI to try again.
 *
 * In retrospect, this shouldn't be TCP/IP sockets, we're not in a stable bi-directional
 * communication with a server, we just want to blast out message to whatever wants
 * to listen.  This I believe is UDP/datagram sockets, but need to read up on that.
 *
 */

#include <JuceHeader.h>

#include "Trace.h"
#include "TraceClient.h"

// the global singleton
class TraceClient TraceClient;

TraceClient::TraceClient()
{
    enabled = false;
    socket = nullptr;
    connectionFailed = false;
}


TraceClient::~TraceClient()
{
    disconnect();
}

void TraceClient::setEnabled(bool b)
{
    if (b != enabled) {
        if (!b) {
            // When we disable, would it be good form to disconnect
            // if we had been previously using it?
            // Seems like it would be,  you generally don't turn this
            // on and off rarndomly
            disconnect();
        }
        enabled = b ;
    }
}

void TraceClient::connect()
{
    if (enabled && !connectionFailed) {
        if (socket != nullptr) {
            if (!socket->isConnected()) {
                // when does this happen?
                // I suppose when the sever window isn't running?
                TraceRaw("TraceClient: Socket not connected, reconnecting\n");
            }
            delete socket;
            socket = nullptr;
        }
            
        if (socket == nullptr) {
            juce::StreamingSocket* maybeSocket = new juce::StreamingSocket();

            // obviously need config options on port
            // second arg is portNumber, third arg is timeOutMillsecs
            bool success = maybeSocket->connect("localhost", 9000, 1000);
            if (!success) {
                TraceRaw("TraceClient: Unable to connect to server\n");
                // at this point we have a few options, see file header notes
                // until we switch to udp/datagram sockets, don't keep trying this
                connectionFailed = true;
            }
            else {
                // first arg true for readyForReading
                // second arg timeout milliseconds
                // unclear from the Juce docs whether this is necessary
                int status = maybeSocket->waitUntilReady(false, 5000);
        
                if (status < 0) {
                    TraceRaw("TraceClient: waitUntilReady error\n");
                }
                else if (status == 0) {
                    TraceRaw("TraceClient: waitUntilReady timeout");
                }
                else {
                    // it looks good enough
                    socket = maybeSocket;
                }
            }
            if (socket != maybeSocket)
              delete maybeSocket;
        }
    }
}

void TraceClient::disconnect()
{
    if (socket != nullptr) {
        TraceRaw("TraceClient: Closing socket\n");
        socket->close();
        delete socket;
        socket = nullptr;
    }
}

void TraceClient::send(const char* msg)
{
    if (msg != nullptr) {
        connect();
        if (socket != nullptr) {
            int expected = (int)strlen(msg);
            int status = socket->write(msg, expected);
            if (status == -1) {
                TraceRaw("TraceClient: Error writing to socket\n");
                // need to close() here?
                delete socket;
                socket = nullptr;
                // try it one more time then bail
                connect();
                if (socket != nullptr) {
                    status = socket->write(msg, expected);
                    if (status == -1) {
                        TraceRaw("TraceClient: Error on retry, no trace for you!\n");
                        delete socket;
                        socket = nullptr;
                    }
                }
            }
            
            if (status >= 0) {
                // didn't fail at least
                if (status != expected) {
                    // what are we supposed to do here?
                    // try the write again with the remainder?
                    char buf[1024];
                    snprintf(buf, sizeof(buf), "TraceClient: Socket write anomoly, expected %d sent %d\n",
                            (int)expected, (int)status);
                    TraceRaw(buf);
                }
            }
        }
    }
}

/****************************************************************************/
/****************************************************************************/
/****************************************************************************/

