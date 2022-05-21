#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <thread>
#include <experimental/filesystem>
#include <sstream>
#include <iterator>
#include <fstream>
#include <sys/types.h>

using namespace std;

int getTrueLength (string buffer);
string getFileType(string path);
string getDataType(string type);
int sendAll(int s, char *buf, int *len);

int main(int argc, char * argv[]){
    int connectSocket; //socket to be able to connect to the server socket
    struct sockaddr_in serverAddr; //address of the server to connect to
    char * connectIP = new char[64];
    int size;

    int port; //specified port or use the default
    if(argc == 2){
        connectIP = argv[1];
        port = 80;
    }
    if(argc == 3){
        connectIP = argv[1];
        port = atoi(argv[2]);
    }

    connectSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);//open the TCP connection
    if (connectSocket == -1) {//error
        printf("Failed to create a client socket");
        exit(-1);//terminate
    }

    memset(&serverAddr, 0, sizeof(serverAddr));//allocating memory
    serverAddr.sin_family = AF_INET;//important info address, family, port
    serverAddr.sin_addr.s_addr = inet_addr(connectIP);
    serverAddr.sin_port = htons(port);

    if (connect(connectSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) == -1){//connect to the server socket
        printf("Failed to connect to server socket");
        exit(-1);
    }

    struct timeval time = {1800, 0};//30 mins default connection time
    if (setsockopt(connectSocket, SOL_SOCKET, SO_RCVTIMEO, (char *) &time, sizeof(time)) == -1){
        printf("Socket timeout error");
        exit(-1);
    }

    pair<string, string> request1 = {"GET","/asw.txt"};//request to send to the server
    /*pair<string, string> request2 = {"POST","file.txt"};//request to send to the server
    pair<string, string> request3 = {"GET","/image.jpeg"};//request to send to the server
    pair<string, string> request4 = {"GET","/page.html"};//request to send to the server
    pair<string, string> request5 = {"POST","image.jpeg"};//request to send to the server
    pair<string, string> request6 = {"POST","page.html"};//request to send to the server*/
    vector <pair<string, string>> requests;
    requests.push_back(request1);
    for (pair<string, string> request : requests){
        if (request.first.compare("GET") == 0){
            string str = "GET " + request.second + " HTTP/1.1" + "\r\n";//GET request format
            str += "\r\n";
            size = strlen(&str[0]);
            int status = sendAll(connectSocket, &str[0], &size);//send
            if (status == -1){//error
                printf("Error sending data");
                exit(-1);
            }
        }
        else if (request.first.compare("POST") == 0){//POST
            string str = "POST / HTTP/1.1\r\n";//POST format
            int length = std::experimental::filesystem::file_size(request.second);//length of the file sent
            string type = getFileType(request.second);//type of the file
            size_t first = request.second.find_last_of('/');
            string filename = request.second.substr(first+1);///name of the file
            str = str + "Content-Type: " + type + "\r\n";//all the headers to sent to the server
            str = str + "Content-Length: " + to_string(length) + "\r\n";
            str = str + "Content-Disposition: inline; filename=\"" + filename + "\"" + "\r\n";
            str = str + "\r\n";
            std::ifstream t(request.second);
            std::string data((std::istreambuf_iterator<char>(t)), std::istreambuf_iterator<char>());
            str = str + data;
            size = strlen(&str[0]);//size of the data
            int status = sendAll(connectSocket, &str[0], &size);//data and size sent
            if (status == -1){
                printf("Error sending data");
                exit(-1);
            }
        }
    }

    bool clientDone = false;
    string fullBuffer = "";
    char buffer[100];
    while (!clientDone ){
        int rcv = recv (connectSocket, buffer, sizeof(buffer), 0);//recieve response
        if (rcv > 1){//print it
            for (int i = 0; i < 100; i++){
                if (buffer[i] == '\0'){
                    break;
                }
                printf("%c", buffer[i]);
            }
            string str = string (buffer, rcv);
            fullBuffer += str;
            bool endOfRequest = true;
            int trueLength = getTrueLength(fullBuffer);
            if (trueLength == -1){
                endOfRequest = false;
            }
            while (endOfRequest){
                string request = fullBuffer.substr(0, trueLength);
                if(trueLength >= fullBuffer.size()){
                    fullBuffer = "";
                }    
                else{
                    fullBuffer = fullBuffer.substr(trueLength);

                }
                string temp = request.substr(0, request.find("\r\n"));
                string token = temp.substr(temp.find(" ")+1,temp.substr(temp.find(" ")+1).find(temp.find(" ")));
                int statusCode = stoi(token);
                if (statusCode != 404){
                    string path = "./";
                    string data = request.substr(request.find("\r\n\r\n")+4);

                    size_t first = request.find("Content-Length: ");
                    string str = request.substr(first, request.find("\r\n", first)-first);
                    istringstream is(str);
                    vector<string> tokens{istream_iterator<string>{is}, istream_iterator<string>{}};
                    string dataSize = tokens[1];
                    int size = stoi(dataSize);

                    size_t firstt = request.find("Content-Type: ");
                    string strr = request.substr(firstt, request.find("\r\n", firstt)-firstt);
                    istringstream iss(strr);
                    vector<string> tokenss{istream_iterator<string>{iss}, istream_iterator<string>{}};
                    string type = tokenss[1];
                    string dataType = getDataType(type);

                    size_t firsttt = request.find("Content-Disposition: ");
                    string strrr = request.substr(firsttt, request.find("\r\n", firsttt)-firsttt);
                    istringstream isss(strrr);
                    vector<string> tokensss{istream_iterator<string>{isss}, istream_iterator<string>{}};
                    string filename = tokensss[2].substr(tokensss[2].find_first_of('\"')+1, tokensss[2].find_last_of('\"') - (tokensss[2].find_first_of('\"')+1));
                    std::ofstream file(path+filename, std::ios::binary);
                    file.write(&data[0],size);
                }
                trueLength = getTrueLength (fullBuffer);
                if (trueLength == -1){
                    endOfRequest = false;
                }                
            }
        }
        else {
            clientDone  = true;
        }
    }
    close(connectSocket);
    return 0;
}

int getTrueLength (string buffer){
    if (buffer.find("\r\n\r\n") == string::npos){
        return -1;
    }
    else {
        string request = buffer.substr(0, buffer.find("\r\n"));
        string token = request.substr(0, request.find(" "));
        if (token.compare("GET") == 0){
            return buffer.find("\r\n\r\n")+4;
        }
        else {
            size_t start = buffer.find("Content-Length: ");
            request = buffer.substr(start, buffer.find("\r\n",start)-start);
            token = request.substr(request.find(" ")+1,request.substr(request.find(" ")+1).find(request.find(" ")));
            int bytes = stoi(token);
            if (buffer.find("\r\n\r\n") + 4 + bytes <= buffer.size()){
                return buffer.find("\r\n\r\n") + 4 + bytes;
            }
            else {
                return -1;
            }
        }
    }
}

string getFileType(string path){
    string extension = path.substr(path.find_last_of('.')+1);
    if (extension.compare("html") == 0) {
        return "text/html";
    }
    else if (extension.compare("jpg") == 0) {
        return "image/jpg";
    }
    else if (extension.compare("jpeg") == 0) {
        return "image/jpeg";
    }
    else if (extension.compare("png") == 0) {
        return "image/png";
    }
    else if (extension.compare("gif") == 0) {
        return "image/gif";
    }
    else if (extension.compare("raw") == 0) {
        return "image/raw";
    }
    else {
        return "text/plain";
    }
}

string getDataType(string type){
    if(type.compare("text/html")== 0){
        return ".html";
    }
    else if(type.compare("image/jpg") == 0){
        return ".jpg";
    }
    else if(type.compare("image/jpeg") == 0){
        return ".jpeg";
    }
    else if(type.compare("image/png") == 0){
        return ".png";
    }
    else if(type.compare("image/gif") == 0){
        return ".gif";
    }
    else if(type.compare("image/raw") == 0){
        return ".raw";
    }
    else{
        return ".txt";
    }
}

int sendAll(int s, char *buf, int *len){
    int total = 0; //bytes sent
    int bytesleft = *len; //bytes left
    int n;
    while (total < *len){
        n = send(s, buf+total, bytesleft, 0);
        if (n == -1){
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total;
    return n==-1?-1:0;
}