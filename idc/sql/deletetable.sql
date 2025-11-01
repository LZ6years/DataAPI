-- 删除T_ZHOBTMIND表1.5天之前的数据。
delete from T_ZHOBTMIND where ddatetime<sysdate-1.5;
commit;
-- 删除T_ZHOBTMIND1表1.5天之前的数据。
delete from T_ZHOBTMIND1 where ddatetime<sysdate-1.5;
commit;
exit;
