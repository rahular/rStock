rStock
======

Stockfish is an open source UCI chess engine, developed by Tord Romstad, Joona Kiiski and Marco Costalba and licensed under the GNU General Public License version 3. The current version 4 (as of August 20, 2013) is available as C++ source code, and also has precompiled versions for Microsoft Windows, Mac OS X, and Linux 32-bit/64-bit. It can use up to thirty-two CPU cores in multiprocessor systems. The maximum size of the transposition tables is eight gigabytes. Stockfish implements an advanced alpha-beta search and uses bitboards.

This project modifies Stockfish and plugs in a neural evaluation function and a companion trainer for purely academic purposes. I thank Andrey Kotlarski without whose BPN, this project would be a lot harder to implement.

How to Install
==============
Installing the engine:
----------------------
`make profile-build ARCH=x86-64`   (This is for 64-bit systems)

`make profile-build ARCH=x86-32`    (This is for 32-bit systems)

Installing the GUI:
------------------
Go to 'Util' and extract the contents of the tarball. Then run the following commands,

`sudo apt-get install tcl-dev`

`sudo apt-get install tk-dev`

`./configure`

`sudo make install`

Installing the trainer:
-----------------------
See the readme for rStockTrainer

Running the GUI
---------------
- Open a terminal and navigate to the 'rStock' directory
- Type the command 'scid'
- In the GUI, go to 'Tools->Analysis engines->New'
- For the 'Name' option, give a suitable name
- For the 'Command' option, browse for the 'rStock' executable and select it
- Leave the other options to defalut
- Click on the 'Configure' button to configure the engine

Some points to remember while configuring the engine:
- There are 4 modes in the engine: 
  - 0 -> Original Stockfish with logging it's thinking lines 
  - 1 -> Online training
  - 2 -> Play with an existing neural network
  - 3 -> Original Stockfish
- For the training mode, the number of threads should be set to 1 (because the write to file cannot happen in parallel)
- The neural network file is usually set to 'stock.bpn'. Therefore make sure that any newly trained NN is named accordingly and placed in the rStock directory

