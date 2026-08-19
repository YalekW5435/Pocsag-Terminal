# Pocsag Encoder for terminals.

A console based POCSAG encoder that encodes alpha, numeric, and tone messages to a 24 bit signed PCM file at 48000hz, written in Gemini Assist's C++ engine, 
without the use of special libraries other than c++ stdlib.
this program was written to solve a lot problems for specific hardware that are unable to transmit on radio, 
or lack the specific functionality.  Message encoding has no limit.
This program went through 15-20 iterations to actually be completely correct.  Recommended not to do what I did, so if you want to improve this utility, you are more than welcome to do so.

The program contains several parameters, all of which are used to encode your messages.  Here is a quick help to rundown the basics:



      
        Options:
       --address <ric>       Single address (0-2097151, anything >> 2097151 gets reset to 0 and recounts)
       --function (0-3)
       --group [10,20-25]  Broadcast to multiple RICs inside braces.  DO NOT put spaces after a comma if you use this option!! (", ").  Leave it like this ",".
       --type <alpha|numeric|tone>
       --message ""      Message string to send.
       --bps <512|1200|2400> Transmission speed (Default: 1200)
       --slot             create a batch of separate messages with different addresses.  
                          When using this, ensure the --slot parameter comes *first* before anything.  
                          Once your first slot is completed, you can move on to the next --slot. 
       --output <file>       Path to 24-bit raw output.

         Required use:
         PocsagEncoder.exe --address --function --bps --type --message ""  --output ""
         PocsagEncoder.exe --group[, or - to repeat addresses sequentially] --function --bps --type --message "" --output "page1.raw"
         PocsagEncoder.exe --slot --address --function --bps --type --message ""  --slot --address --function --bps --type --message "" --slot --address --function --bps --type --message ""  --output "page1.raw"
         
         I would highly recommend using this required use to get a feel of how this functionality works. 

        
# Disclaimer:
if you wish to use this program on SDR, depending on where youre located on Earth, or if you wish to alert pagers, please do so with caution.  
I am not responsible for the program being used maliciously, so please encode at your own risk.  

# Things worth noting:
If youre encoding numeric messages to disk and you do not see a message in PDW.exe while decoding, this is because PDW's newer versions default numeric messages to function #'s.  It attempts to guess, even with passive decoding of other transmitters on the air.  Alphanumeric messages do work fine, most of the time, and is because buffer issues tend to arise in PDW.  You will need to use another decoder to decode the numeric messages because PDW isnt the best solution for most of these applications, especially on windows. best bet is to use SoRFMon's decoder, multimon, including SDR# community plugins that allow for POCSAG decoding, as these will probably have 0 issues.  

# Final notes:

I hope this program makes of good use to those who want to experiment a little.  But as mentioned in the Disclaimer, tread carefully on the RF.  

