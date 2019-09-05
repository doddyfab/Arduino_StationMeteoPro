# Arduino_StationMeteoPro
La station est basée sur un Arduino Uno, couplée à un shield Ethernet (le filaire y’a que ca de vrai) qui servira aussi de lecteur carte micro SD.

## Les éléments qui composeront la station sont :

* Arduino Uno R3
* kit Lextronic Girouette/Anémomètre/Pluviomètre avec interface Grove
* sonde BME280 pour température/humidité/baromètre
* plus tard se rajoutera une sonde de rayonnement solaire RG100

## Les données seront traitées comme suit :

* L’arduino génèrera les données toutes les min (voir ci après)
* Il stockera la données dans un fichier CSV horodaté par jour
* Il appellera une URL sur mon Synology pour stocker les données sur le synology (backup)
* Il stockera via une URL Synology les données dans une base MySQL qui servira au site météo

## Le projet comportera aussi toute la partie serveur web avec un site complet d’accès aux données avec :

* des infos lives
* des prévisions de pluie sur 12h, de temps sur 5j
* des infos vigilance météo france
* la qualité de l’air
* les pollens
* une image webcam horodatée toutes les 5 min
* éphémérides
* des statistiques mensuelles, annuelles, …

# Gestion des capteurs
## Girouette

Le programme lira la valeur analogique (16 valeurs analogiques correspondant à 16 positions sur la rose des vents, produite par des ILS et résistances) de la girouette toutes les 3 sec et stockera cette valeur sous forme d’un angle (0-360°). Au bout d’une minute, le programme fera une moyenne des valeurs obtenues et définira la direction moyenne du vent pendant 1 min.

## Anémomètre

Le capteur est fait sur une base ILS. Le programme va compter toutes les 3 sec le nombre de fermetures de l’aimant de l’ILS (1 passage = 1 fermeture). On calculera en fonction du nombre de passage en 3 sec la vitesse du vent. On échantillonne en continu sur 1 min, puis on fait la moyenne des valeurs récoltées pour en déduire le vent moyen sur 1 min. La valeur maximale durant la minute sera la rafale.

## Pluviomètre

Le pluviomètre est basé sur un ILS qui se ferme à chaque basculement d’auget. Chaque auget fait tomber l’équivalent de 0,2794mm de pluie (1mm = 1l/m2 au sol). Le programme compte pendant 1 min le nombre de bascule et calcule la quantité de pluie en 1 min.

## Température

La température est récupéré sur le capteur toutes les 10 sec. Le programme va ensuite faire la moyenne de ces valeurs et définir la température moyenne sur 1 min.
Humidité et point de rosée

Comme pour la température, le programme va lire le capteur toutes les 10 sec et au bout d’1 min, faire la moyenne des valeurs obtenues pour avoir l’humidité moyenne sur 1 min. A partir de l’humidité et de la température, le serveur web calculera le point de rosée.

## Pression atmosphérique

Le BME280 permet de récupérer la pression atmosphérique en fonction de la température, et de l’altitude. Nous exploiterons donc cette donnée. Le programme lira le capteur toutes les 10 sec et au bout d’1 min, fera la moyenne des valeurs obtenues et définira la pression moyenne sur 1 min.
