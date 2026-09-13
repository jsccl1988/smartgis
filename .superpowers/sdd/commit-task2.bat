@echo off
git add src/sdb/datasource/gdal/ogr_feature_codec.h src/sdb/datasource/gdal/ogr_feature_codec.cc src/sdb/datasource/gdal/ogr_feature_kind.h src/sdb/datasource/gdal/sde_gdal_test.cc src/sdb/datasource/gdal/BUILD.gn .superpowers/sdd/task-2-brief.md .superpowers/sdd/task-2-report.md .superpowers/sdd/progress.md
git -c user.name=jsccl1988 -c user.email=jsccl1988@.com commit -F .superpowers/sdd/task-2-commit-msg.txt
exit /b %ERRORLEVEL%
