#include <string>
#include "./httplib.h"
#include "./nlohmann/json.hpp"
#ifndef SCOREBOARD_HPP
#define SCOREBOARD_HPP


namespace SB
{

const int NUM_SCORES = 10;

class Client {

private:

    std::string apiKey;
    httplib::Client* httplibClient;
    const char delimiter = ',';

public:

    Client(const std::string& key) {
        apiKey = key;
        httplibClient = new httplib::Client("http://myfs.dev");
    }


    ~Client() {
        delete httplibClient;
    }


    int GetScores(int* scoreArray) {

        auto url = std::string("/sb/api.php?uuid=") + apiKey;

        // "/sb/api.php?uuid=big_long_string"

        auto res = httplibClient->Get(url);

        if (res && res->status == 200) {

            if (res->body.size() == 0)
                return 0;

            DestringifyScores(res->body, scoreArray);

			/// Debug
            //for (int i = 0; i < SB::NUM_SCORES; i++) {
            //    std::cout << "Score #" << i << " = " << scoreArray[i] << std::endl;
            //}
        }
        else {
            std::cout << "Error: " << res.error() << std::endl;
            return 0;
        }

        return 1;
    }


    int SaveScores(int* scoreArray) {

        auto url = std::string("/sb/api.php?uuid=") + apiKey;

        //"/hs/api.php?uuid=645a2f06-e1e4-46b4-8f13-403bcba259cc&scores=<score_string>&somethingelse=10&anotherone=20"
        
        url.append("&scores=");
        url.append(StringifyScores(scoreArray));

        auto res = httplibClient->Get(url);

        //if (res && res->status == 200) {
         //   std::cout << res->body << std::endl;
        //}
        //else {
        //    std::cout << "Error: " << res.error() << std::endl;
         //   return 0;
       // }

        return 1;
    }
   

    void DestringifyScores(std::string& src, int* scoreArray) {

        nlohmann::json result = nlohmann::json::parse(src);

        std::string line = result["scores"].begin().value();
        std::stringstream ss(line);
        std::string tmp;

        for (int i = 0; i < SB::NUM_SCORES; i++) {
            ss >> scoreArray[i];
            getline(ss, tmp, delimiter);
        }
    }


    std::string StringifyScores(int* scoreArray) {

        std::string result;

        for (int i = 0; i < SB::NUM_SCORES; i++) {
            result += std::to_string(scoreArray[i]);
            if (i < SB::NUM_SCORES - 1)
                result += delimiter;
        }

        return result;
    }

};


}

#endif // SCOREBOARD_HPP