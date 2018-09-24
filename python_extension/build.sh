g++ -fPIC -I/home/skumaran/boost/include -I/opt/rh/rh-python36/root/include -I/opt/rh/rh-python36/root/usr/include/python3.6m/ -I../include -I../parser/include ivy_python_extension.cpp ../ivyenglib.a -shared -lboost_python -o ivypy.so

