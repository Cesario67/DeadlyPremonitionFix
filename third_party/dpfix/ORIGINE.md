# DPfix (Durante / Peter Thoman) : origine des fichiers

- Projet : https://github.com/PeterTh/dpfix
- Version importée : **0.9**, commit `050c562ba57f7e7b0d799cfcb7c993e1f609714f` (17/11/2013),
  dernière version dont les sources sont publiées. La 0.9.5 (03/12/2013, compatibilité DP.exe 1.01b)
  n'existe qu'en binaire.
- Licence : GPL-3.0-or-later (voir `LICENSE.txt` et les en-têtes des fichiers), Copyright 2013
  Peter Thoman (Durante).

## Import

Le premier commit de ce dossier contient les fichiers **tels quels**. Toute modification ultérieure
est faite dans des commits séparés et signalée en commentaire dans les fichiers concernés
(« Modifié pour DPStabilityFix : ... »), conformément à la GPL (section 5a).

## Fichiers volontairement non importés

| Fichier | Raison |
|---|---|
| `dinput.h` | En-tête du SDK DirectX de Microsoft, pas sous GPL. |
| `dinputWrapper.cpp/.h` | Chaînage d'un wrapper DirectInput, inutile ici. |
| `pack/dpfix/FXAA.h`, `pack/dpfix/FXAA.fx` | Shader NVIDIA FXAA 3.11 : « ALL RIGHTS RESERVED », aucune autorisation de redistribution. Non libre : l'anticrénelage passe par SMAA. |
| `DPfix.sln`, `DPfix.vcxproj`, `DPfix.v11.suo` | Projet Visual Studio 2012 remplacé par CMake. |
| `pack/dpfix/screens`, `tex_dump`, `tex_override` | Dossiers vides créés à l'exécution. |

## Licences des fichiers tiers inclus

| Fichier | Auteur | Licence |
|---|---|---|
| `src/AreaTex.h`, `src/SearchTex.h`, `shaders/SMAA.h`, `shaders/SMAA.fx` (en-tête) | Jimenez, Masia, Echevarria, Sousa, Gutierrez | Licence de type MIT (en-tête des fichiers) |
| `shaders/VSSAO.fx`, `shaders/VSSAO2.fx` | Tomerk (OBGE), adapté par Durante | **À vérifier** : aucune licence indiquée dans les fichiers |
| `shaders/GAUSS.fx` | Durante, d'après un article de rastergrid.com | GPL-3.0-or-later (projet DPfix) |
