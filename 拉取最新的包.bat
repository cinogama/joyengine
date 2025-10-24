rmdir /s /q pkg
rmdir /s /q build\pkg
rmdir /s /q 3rd\pkg

cd 3rd/woolang
git checkout release
git pull
cd ../..
call baozi install
pause
