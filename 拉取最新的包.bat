cd 3rd/woolang
git checkout release
git pull
cd ../..
call baozi install -nonative
call baozi install -nonative -profile debug
pause
