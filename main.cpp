#include <cmath>
#include <cstring>
#include <iostream>
#include <stdio.h>
#include <vector>
#define earthRadiusKm 6371.0
#define lat_home 50.500025077185455
#define lon_home 9.698673141000365

struct Flight {
    char* name;
    std::vector<double> dists;
};

double deg2rad(double deg)
{
    return (deg * M_PI / 180);
}

double rad2deg(double rad)
{
    return (rad * 180 / M_PI);
}

void addFlight(char* name, double dist, std::vector<Flight> flights)
{
    for (auto flight : flights) {
        if (!std::strcmp(name, flight.name)) {
            flight.dists.push_back(dist);
            return;
        }
    }
    std::vector<double> dists;
    dists.push_back(dist);
    struct Flight flight = { name, dists };
    flights.push_back(flight);
}

double distanceEarth(double lat1d, double lon1d, double lat2d, double lon2d)
{
    double lat1r, lon1r, lat2r, lon2r, u, v;
    lat1r = deg2rad(lat1d);
    lon1r = deg2rad(lon1d);
    lat2r = deg2rad(lat2d);
    lon2r = deg2rad(lon2d);
    u = sin((lat2r - lat1r) / 2);
    v = sin((lon2r - lon1r) / 2);
    return 2.0 * earthRadiusKm * std::asin(std::sqrt(u * u + std::cos(lat1r) * std::cos(lat2r) * v * v));
}

char* getName(std::string name)
{
    char* n = (char*)std::malloc(name.length());
    std::memcpy(n, name.c_str(), name.length() - 1);
    return n;
}

int main()
{

    std::string input;
    char* name;
    double lat, lon;
    bool lat_b = 0, lon_b;
    while (std::cin >> input) {
        if (input[0] == '*') {
            name = getName(input);
            std::cout << name << " : " << input << std::endl;
        }
        if (input == "Latitude") {
            std::cin >> input;
            std::cin >> input;
            lat = stod(input);
            lat_b = 1;
        } else if (input == "Longitude:") {
            std::cin >> input;
            lon = stod(input);
            lon_b = 1;
        } else if (lat_b && lon_b) {
            lat_b = lon_b = 0;
            //std::cout << "Distanz von Flug " << name << " nach hause: " << distanceEarth(lat, lon, lat_home, lon_home) << std::endl;
        }
    }
    std::cout << "heyhey";
}