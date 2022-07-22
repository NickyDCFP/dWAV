# dWAV: Command-Line WAV File Disassembler
dWAV disassembles .wav files and outputs a formatted version of their human-readable data. It can also modify the data and copy the new contents into another file. In any case, dWAV will never modify the infile. 

## Flags
dWAV operates under a flag system that determines what alterations it should make to the file

* When the tool is called without any flags, `dwav`, it opens the file at the default input filename `input.wav` and prints its data.

* `dwav -i infile.wav` allows the user to input a custom input filename. In this case, dWAV will open `inputfile.wav` and print its data.

* `dwav -o outfile.wav` allows the user to input a custom output filename. In this case, dWAV will open the file at the default input filename `input.wav`, print its contents, and copy it to `outfile.wav`. dWAV will always print the file data to the outfile as long as an outfile is specified.

* `dwav -c` ensures that the file will be copied regardless of whether or not an outfile or any alterations are specified. In this case, dWAV will open the file at the default filename `input.wav`, print its contents, and copy it to the default output filename `output.wav`. The `-c` flag is only impactful if no alterations and no outfile are specified.

* `dwav -hz 48000` will change the sample rate of the data to 48000 or whatever positive nonzero integer the user specifies, change the byte rate to compensate, and write the data to the desired outfile, in this case the default outfile `output.wav`.

* `dwav -r` will reverse the contents of the file (the audio samples) and write the new data to the outfile, in this case the default outfile at `output.wav`

## Sample Usage
* `dwav -i PartitaEMajor.wav -o ReversedSpeed.wav -hz 96000 -r` will read data in from the file at `PartitaEMajor.wav`, print its data, change its sample rate to 96000 (scaling its byte rate accordingly), reverse the samples, and write the new data to `ReversedSpeed.wav`.

* `dwav -i file.wav -c` will open the file at `file.wav`, print its data, and copy it to the default outfile at `output.wav`.
