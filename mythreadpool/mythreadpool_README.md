# mythreadpool较为复杂版本
本文件夹下，实现的mythreadpool较为复杂，Any类，信号量类均不使用库文件
## 编译为动态库
* 1 生成.so文件  
`g++ -fPIC -shared threadpool.cpp -o libtdpool.so -std=c++17`   
**-fPIC**表示告诉编译器生成的动态库，生成与位置无关的代码(Position-Independent Code,PIC)，他不依赖固定的内存地址，可以在内存任何位置加载和执行       
**-shared**表示生成一个动态链接库(shared library)  
**libtdpool.so**库的命名规则为lib<库名>.so  


* 2 移动.so文件，.h文件
`sudo mv libtdpool.so /usr/local/lib/`  
把.so文件移动到/usr/local/lib/  
`sudo mv threadpool.h /usr/local/include/`

* 3 编译文件,别忘了链接ltdpool库和lpthread  
` g++ test_thread_pool.cpp -std=c++17 -ltdpool -lpthread`

* 4 添加链接位置  
如果直接运行./a.out，运行报错不能发现libtdpool,`error while loading shared libraries: libtdpool.so: cannot open shared object file: No such file or directory`.  
因为运行时候要寻找链接库   
`vim /etc/ld.so.conf `: include /etc/ld.so.conf.d/*.conf 包含了所有.so文件，cd到/etc/ld.so.conf.d/下，有很多配置文件，`vim`一个关于我们自己库的。  
`vim mylib.conf`输入vim /usr/local/lib/ 位置，最后`loconfig`，刷新配置文件

