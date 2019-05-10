#!/bin/bash

# 实现将远程数据库sql导入到本地数据库且将sql文件存放本地目录便于以后传输

# target mysql info
mysql_host_remote="192.168.0.100"
mysql_user_remote="root"
mysql_password_remote="123"

# local mysql info
mysql_host_local="localhost"
mysql_user_local="root"
mysql_password_local="123"

# Mysql backup directory
backup_dir="./databases_save"
#source_dir="/var/lib/mysql/databases_save"
if [ ! -d $backup_dir ]; then
    mkdir -p $backup_dir
fi

cd $backup_dir

# 备份的数据库数组
all_dbs=$(echo "show databases;" | mysql -u$mysql_user_remote -p$mysql_password_remote -h$mysql_host_remote)

#不需要备份的单例数据库
db_not_needed=(auth_system course_system file_system knowledge_system quartz question_system tssanalysis tssexam tssinfocenter user_system video_system zipkin paper_system log_system)

date=$(date -d '+0 days' +%Y%m%d)

#zip 打包
#zipname="sql_save_"$date".zip"


#循环备份并且导入到本地数据库
for dbname in ${all_dbs}
do
    if [[ ${db_not_needed[*]} =~ $dbname ]]; then
	sqlfile=$dbname-$date".sql"
	
	mysqldump --add-drop-database -u$mysql_user_remote -p$mysql_password_remote -h$mysql_host_remote -B $dbname > $sqlfile
	
	## 导入之前先在本地建库
	$(echo "drop database "$dbname | mysql -u$mysql_user_local -p$mysql_password_local -h$mysql_host_local)
	$(echo "create database "$dbname | mysql -u$mysql_user_local -p$mysql_password_local -h$mysql_host_local)
    echo "creating:$dbname"
	#mysqldump -u$mysql_user_local -p$mysql_password_local -h$mysql_host_local $dbname < $sqlfile
	$(echo "source "$sqlfile | mysql -u$mysql_user_local -p$mysql_password_local -h$mysql_host_local)
    fi
done

