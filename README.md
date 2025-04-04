Compile with:
1. **gcc keylogger.c -o keylogger.exe -lws2_32** 
2. **gcc server.c -o server.exe -lws2_32 -lgdi32 -mwindows**
3. **gcc -mwindows keylogger.c -o keylogger.exe -lws2_32 -ladvapi32**
