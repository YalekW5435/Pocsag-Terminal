# Pocsag Encoder for terminals. v1.5.

A console based POCSAG encoder that encodes alpha, numeric, and tone messages to a 24 bit signed PCM file at 48000hz, written in Gemini Assist's C++ engine, 
without the use of special libraries other than c++ stdlib.
this program was written to solve a lot problems for specific hardware that are unable to transmit on radio, 
or lack the specific functionality.  Message encoding has no limit.
This program went through 15-20 iterations to actually be completely correct.  Recommended not to do what I did, so if you want to improve this utility, you are more than welcome to do so.

This program has been updated from the v1.0 to include input file IO, with the ability to bypass the 8191 character limit, along with automation features.  
I have revamped the help screen (legitimately), to explain the additional functionalities.  Please print such to the screen to see whats changed.


        
# Disclaimer:
if you wish to use this program on SDR, depending on where youre located on Earth, or if you wish to alert pagers, please do so with caution.  
I am not responsible for the program being used maliciously, so please encode at your own risk.  

# Things worth noting:
If youre encoding numeric messages to disk and you do not see a message in PDW.exe while decoding, this is because PDW's newer versions default numeric messages to function #'s.  It attempts to guess, even with passive decoding of other transmitters on the air.  Alphanumeric messages do work fine, most of the time, and is because buffer issues tend to arise in PDW.  You will need to use another decoder to decode the numeric messages because PDW isnt the best solution for most of these applications, especially on windows. best bet is to use SoRFMon's decoder, multimon, including SDR# community plugins that allow for POCSAG decoding, as these will probably have 0 issues.  

# Final notes:

I hope this program makes of good use to those who want to experiment a little.  But as mentioned in the Disclaimer, tread carefully on the RF.  

