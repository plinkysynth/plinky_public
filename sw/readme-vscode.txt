# plinky code vscode setup with stlink-v3

Details for MacOS so far.  


Install in vscode:  
	STM32 VS Code Extension  
	Cortex-Debug


Edit `cmake/st-project.cmake`  
	- Comment out `emu` items ` under `target_sources`  


Make sure you have a new version of openocd  

`brew install openocd`  

Edit `settings.json`: 
	- set correct path to openocd - must be version above v11  
	- this should be `"/opt/homebrew/bin/openocd"` with brew on Apple Silicon machines  




