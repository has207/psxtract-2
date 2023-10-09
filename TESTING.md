Testing
=======

There are no tests included in the code so when making changes it is important to back test that
we don't break existing functionality. Here are some code paths that should be tested with example eboots:

- Single disc game, no audio (vast majority of titles)
- Single disc game, CDDA audio (Gaia Seed is good candidate, small file size and CDDA track plays as the game starts, other titles: Advanced V.G.2, Capcom Generation Vol 5, Castlevania SOTN, Community POM, Cotton 100%, Mad Stalker, Panzer Bandit, Pocket Fighter, Silhouette Mirage, Tron Bonne, Vib-Ribbon))
- Multi disc game, no audio (Armored Core: Master of Arena, Fear Effect 1 & 2, G-Police, MGS, Parasite Eve, Policenauts, Rival Schools, Strider 2)
- Multi disc game, CDDA audio (AitD:New Nightmare, Langrisser IV&V, Legend of Dragoon(JP), Maboroshi Tsukiyo, Soukuu no Tsubasa Gotha World)
- Multi disc game, CDDA audio missing in EBOOT (RE2 Dualshock Edition)
- CDDA tracks with short pregaps (Tsuukai Slot Shooting, GTA)
- CDDA tracks with long pregaps ('99 Koushien, A. IV Evolution Global, Bowling (US), Centipede (US/EU), Dai-4-Ji Super Robot Taisen S, Hanabi: Fantast, Jet Copter X, KoF '96, KoF Kyo, Koushien V, Motteke Tamago with Ganbare, Perfect Weapon, Touge Max Saisoku Drift Master, Vib-Ribbon, Yamasa Digi Guide Hyper Rush)
- CDDA tracks with large pregap on first audio track only (Chou Aniki Kyuukyoku, Nyan to Wonderful, Slam Dragon, Yamasa Digi Guide New Pulsar R, Yamasa Digi Guide M771)


In order to test:

- run psxtract on the relevant eboot, don't specify -c to keep TEMP files
- compare CUE file to data for the game on redump.org, redump assumes all tracks are separate BIN files whereas we generate a single BIN, so use CDMage to load real disc dump then Save it in a new directory to create single BIN and compare the CUE files directly
- ensure all tracks are the correct length, look at D0X_TRACK_XX.BIN files in TEMP
- ensure pregap values in the CUE match redump
- ensure data track passes md5 check by adding the game in Duckstation (then properties/check hashes)
- ensure audio looks correct by importing track from real disc and TEMP directory created during psxtract process into Audacity (File/Import/Raw Data - Signed 16-bit PCM, Little-endian, 2 Channel (stereo), 44100Hz)
