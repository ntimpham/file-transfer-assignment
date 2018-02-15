#define SUCCESS_CODE 0
#define ERROR_CODE -1

#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>

using namespace std;

/* Helpers */
string parseKey(string);
void getFile(string, string, int);
void putFile(string, string, int, int);
void connectToServer();

/* Globals */
struct sockaddr_in clientAddr;
struct sockaddr_in serverAddr;
int clientFD;
string host;
int port;
string command;
char code;
string key;
char commandMsg[9] = { '\0' };


/**
 * Main Function
 */
int main(int argc, char *argv[]) {
  try {
    #if DEBUG
    cout << "Client is running." << endl;
    #endif

    if (argc < 3 + 1) throw "Expected atleast 3 params.\n(host, port, command)";
    // Get host, port, and command
    host = argv[1];
    port = stoi(argv[2]);
    command = argv[3];
    // Parse out code (G, P, or F)
    code = command[0];
    // Get key
    if (code != 'F') {
      key = parseKey(command);
      // Verify key length
      if (key.length() < 1 || key.length() > 8) throw "Key must be 1-8 ASCII characters.";
    }
    // Generate command message
    for (unsigned int i = 0; i < command.length(); ++i) {
      commandMsg[i] = command[i];
    }

    #if DEBUG
    cout << "Host: " << host << endl;
    cout << "Port: " << port << endl;
    cout << "Command: " << command << endl << endl;
    #endif

    switch (code) {
      case 'G':
        {
          // Verify number of args
          if (argc != 5 + 1) throw "Expected 5 params.\n(host, port, command, filename, recv size)";
          // Get filename, and recv size
          string filename = argv[4];
          int recvSize = stoi(argv[5]);
          
          connectToServer();
          // Call helper
          getFile(key, filename, recvSize);
        }
        break;
      case 'P':
        {
          // Verify number of args
          if (argc != 6 + 1) throw "Expected 6 params.\n(host, port, command, filename, send size, wait time)";
          // Get filename, send size, and wait time
          string filename = argv[4];
          int sendSize = stoi(argv[5]);
          int waitTime = stoi(argv[6]);

          connectToServer();
          // Call helper
          putFile(key, filename, sendSize, waitTime);
        }
      break;
      case 'F':
        connectToServer();
        #if DEBUG
        cout << "Server will now terminate." << endl;
        #endif
        break;
      default:
        throw "Unknown command. Expected G, P, or F.";
    }
  }
  catch(const char *e) {
    cerr << e << endl;
    return ERROR_CODE;
  }
  #if !DEBUG
  catch(...) {
    cerr << "Unexpected error occured." << endl;
    return ERROR_CODE;
  }
  #endif
}


/**
 * Helpers
 */
string parseKey(string command) {
  return command.substr(1, command.length() - 1);
}


void getFile(string key, string filename, int recvSize) {
  #if DEBUG
  cout << "Doing GET" << endl;
  cout << "Key: " << key << endl;
  cout << "Filename: " << filename << endl;
  cout << "Receive Size: " << recvSize << endl << endl;
  #endif
  vector<char> buffer;
  int bytesReceived;
  ofstream outFile;
  
  try {
    outFile.open(filename, ios::binary | ios::trunc);
    if (!outFile.is_open()) throw "Could not create the file.";

    do {
      buffer.clear();
      buffer.assign(recvSize, 0x0);
      // Receive file chunk
      bytesReceived = recv(clientFD, buffer.data(), recvSize, 0);
      if (bytesReceived < 0) throw "An error occurred while downloading the file.";
      // Write to file
      outFile.write(buffer.data(), bytesReceived);
      // Check state
      if (outFile.fail()) throw "An error occurred while writing the file.";
      #if DEBUG
      cout << "Received " << bytesReceived << " bytes." << endl;
      #endif
    } while (bytesReceived > 0);
    // Cleanup
    outFile.close();
    close(clientFD);
  }
  catch (...) {
    outFile.close();
    close(clientFD);
  }
  #if DEBUG
  cout << "Download complete." << endl;
  #endif
}


void putFile(string key, string filename, int sendSize, int waitTime) {
  #if DEBUG
  cout << "Doing PUT" << endl;
  cout << "Key: " << key << endl;
  cout << "Filename: " << filename << endl;
  cout << "Send Size: " << sendSize << endl;
  cout << "Wait Time: " << waitTime << endl << endl;
  #endif
  vector<char> buffer;
  int bytesRead;
  ifstream inFile;

  try {
    inFile.open(filename, ios::binary);
    if (!inFile.is_open()) throw "Could not open the provided file.";

    while (inFile) {
      buffer.clear();
      buffer.assign(sendSize, 0x0);
      inFile.read(buffer.data(), sendSize);
      bytesRead = inFile.gcount();
      if (!bytesRead) break; // Stop when nothing is read
      // Send file chunk to server
      if (send(clientFD, buffer.data(), bytesRead, 0) < 0) {
        throw "An error occurred while uploading to the server.";
      }
      #if DEBUG
      cout << "Sent " << bytesRead << " bytes." << endl;
      #endif
      usleep(waitTime * 1000);
    }
    if (!inFile.eof() && inFile.fail()) throw "An error occurred while reading the file.";
    // Cleanup
    inFile.close();
    close(clientFD);
  }
  catch (...) {
    inFile.close();
    close(clientFD);
    throw;
  }
  #if DEBUG
  cout << "Upload complete." << endl;
  #endif
}


void connectToServer() {
  // Fill out server address
  memset(&serverAddr, '0', sizeof(serverAddr));
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(port);
  // Get server hostname entry
  struct hostent *hostEntry = gethostbyname(host.c_str());
  if (hostEntry  == NULL) {
    throw "Failed to obtain server hostname.";
  }
  // Iterate until last address entry
  struct in_addr **hostAddrs = (struct in_addr **) hostEntry->h_addr_list;
  for(int i = 0; hostAddrs[i] != NULL; ++i) {
    serverAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*hostAddrs[i]));
  }
  // Create socket
  if ((clientFD = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    throw "Failed to create a socket.";
  }
  // Connect to server
  if (connect(clientFD, (struct sockaddr *) &serverAddr, sizeof(serverAddr))) {
    throw "Failed to connect to server.";
  }
  // Send command
  if (send(clientFD, &commandMsg, sizeof(commandMsg), 0) < 0) {
    throw "Could not send terminate command.";
  }
}
