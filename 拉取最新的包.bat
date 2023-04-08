del /q .\build\builtin\api\*
del /q .\build\builtin\editor\*
cd 3rd/woolang
git checkout release
git pull
cd ../..
baozi install