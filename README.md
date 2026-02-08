# Pocsag Encoder for terminals.

A console based POCSAG encoder that encodes alpha, numeric, and tone messages to a 24 bit signed PCM file at 48000hz, written in Gemini Assist's C++ engine, 
without the use of special libraries other than c++ stdlib.
this program was written to solve a lot problems for specific hardware that are unable to transmit on radio, 
or lack the specific functionality.  Message encoding has no limit.


DISCLAIMER: if you wish to use this program on SDR, depending on where youre located on Earth, or if you wish to alert pagers, please do so with caution.  
I am not responsible for the program being used maliciously, so please encode at your own risk.  

The program contains several parameters, all of which are used to encode your messages.  Here is a quick help to rundown the basics:
      
        Options:
       --address <ric>       Single address (0-2097151, anything >> 2097151 gets reset to 0 and recounts)
       --function (0-3)\n
       --group [10,20-25]  Broadcast to multiple RICs inside braces
       --type <alpha|numeric|tone>
       --message ""      Message string to send.
       --bps <512|1200|2400> Transmission speed (Default: 1200)
       --slot             create a batch of separate messages with different addresses.  
                          When using this, ensure the --slot parameter comes *first* before anything.  
                          Once your first slot is completed, you can move on to the next --slot. 
       --output <file>       Path to 24-bit raw output.
     
     --output <file>       string, Path to 24-bit raw output.
         Required use:
         PocsagEncoder.exe --address --function --bps --type --message ""  --output ""
         PocsagEncoder.exe --group[, or - to repeat addresses sequentially] --function --bps --type --message "" --output "page1.raw"
         PocsagEncoder.exe --slot --address --function --bps --type --message""  --slot --address --function --bps --type --message "" --slot --address --function --bps --type --message ""  --output "page1.raw"
         
         I would highly recommend using this required use to get a feel of how this functionality works. 

        
