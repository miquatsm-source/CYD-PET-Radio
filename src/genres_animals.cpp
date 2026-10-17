#include <Arduino.h>
#include "genres_animals.h"

// Ten common music genres and their assigned animals for the CYD.
const MusicGenreAnimal genreList[10] = {
    {
        "Pop",
        "Peacock",
        "Poldi",
        "A colorful bird that loves the spotlight and showing off its magnificent feathers.",
        200
    },
    {
        "Rock",
        "Lion",
        "Leopold",
        "The king of the savanna, with a wild mane and a powerful voice and presence.",
        200
    },
    {
        "Hip-Hop",
        "Kangaroo",
        "Hoppe",
        "A dynamic animal that hops to the rhythm and always has a feel for the beat.",
        200
    },
    {
        "Techno",
        "Chameleon",
        "Egon",
        "An adaptable reptile that changes to bright neon colors in time with the bass.",
        200
    },
    {
        "Classical",
        "Swan",
        "Ludwig",
        "An elegant waterbird that glides across the lake with the grace of a gentle symphony.",
        200
    },
    {
        "Jazz",
        "Cat",
        "Moritz",
        "A smooth, cool cat known for late-night strolls and improvised climbing sessions.",
        200
    },
    {
        "Heavy Metal",
        "Rhino",
        "Siegfried",
        "A massive, armored powerhouse that charges forward with unstoppable force.",
        200
    },
    {
        "Reggae",
        "Sloth",
        "Willi",
        "An extremely laid-back creature that enjoys life in the treetops at an easy offbeat pace.",
        200
    },
    {
        "Country",
        "Horse",
        "Blitz",
        "A loyal companion of the prairie who loves freedom and acoustic guitar tunes.",
        200
    },
    {
        "German Pop",
        "Budgie",
        "Hansi",
        "A cheerful, sociable bird that chirps happy tunes and brings the party.",
        200
    }
};

// Print one animal entry to the serial monitor.
void printGenreDetails(int index) {
    if (index >= 0 && index < 10) {
        Serial.print("Genre: "); Serial.println(genreList[index].genre);
        Serial.print("Animal: "); Serial.print(genreList[index].animalType);
        Serial.print(" (Name: "); Serial.print(genreList[index].animalName); Serial.println(")");
        Serial.print("Description: "); Serial.println(genreList[index].description);
        Serial.print("Display height: "); Serial.print(genreList[index].displayHeight); Serial.println(" px");
        Serial.println("----------------------------------------");
    }
}
