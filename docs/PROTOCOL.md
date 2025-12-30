# PROTOCOL (Client <-> Server)

Version: v1.0  
Transport: TCP  
Framing: 一行一个 JSON（`\n` 结尾）  
二进制：文件数据块用 base64 放在 JSON 字段中

错误码与含义同 `docs/INTERFACES.md`。

---

## 通用回应格式
```json
{"ok":true,"code":0,"msg":"OK","data":{}}
```
出错示例：
```json
{"ok":false,"code":-2,"msg":"ENOENT","data":{}}
```

---

## 认证与角色
- 角色：`author` / `reviewer` / `editor` / `admin`。
- 登录后返回 `token` 和角色；后续请求必须携带 `token`，服务器做权限检查。
- 最低限度：未登录只能 `PING` / `LOGIN`。

### PING
Req
```json
{"op":"PING"}
```
Resp
```json
{"ok":true,"code":0,"msg":"PONG","data":{"version":"v1.0"}}
```

### LOGIN
Req
```json
{"op":"LOGIN","user":"alice","password":"123456"}
```
Resp
```json
{"ok":true,"code":0,"msg":"OK","data":{"token":"t_xxx","role":"author"}}
```

### LOGOUT
Req
```json
{"op":"LOGOUT","token":"t_xxx"}
```
Resp 同通用格式。

---

## 文件系统操作（映射 VfsAPI）
- 路径由服务器按 `INTERFACES.md` 的规则规范化后再处理。
- 读写为分块流式：客户端控制 `off`/`n` 循环请求。
- 典型块大小建议 4096B，但协议不强制。

### MKDIR
```json
{"op":"MKDIR","token":"t_xxx","path":"/papers"}
```
Resp 通用格式。

### RMDIR
```json
{"op":"RMDIR","token":"t_xxx","path":"/papers"}
```
仅空目录可删。

### LIST
Req
```json
{"op":"LIST","token":"t_xxx","path":"/papers"}
```
Resp
```json
{"ok":true,"code":0,"msg":"OK","data":{"names":["paper_a1b2c3"]}}
```

### CREATE（创建空文件）
```json
{"op":"CREATE","token":"t_xxx","path":"/papers/p1/v1.pdf"}
```

### UNLINK（删除文件或空目录入口）
```json
{"op":"UNLINK","token":"t_xxx","path":"/papers/p1/v1.pdf"}
```

### READ（分块下载）
Req
```json
{"op":"READ","token":"t_xxx","path":"/papers/p1/v1.pdf","off":0,"n":4096}
```
Resp
```json
{"ok":true,"code":0,"msg":"OK",
 "data":{"off":0,"n":1234,"eof":false,"b64":"AAAA..."}}
```

### WRITE（分块上传）
Req
```json
{"op":"WRITE","token":"t_xxx","path":"/papers/p1/v1.pdf","off":0,"b64":"AAAA..."}
```
Resp
```json
{"ok":true,"code":0,"msg":"OK","data":{"written":4096}}
```

### TRUNCATE（可选）
```json
{"op":"TRUNCATE","token":"t_xxx","path":"/papers/p1/v1.pdf","size":0}
```

---

## 备份接口（管理员）
- 备份实现为整个设备快照；恢复会覆盖现有数据。

### BACKUP_CREATE
```json
{"op":"BACKUP_CREATE","token":"t_admin","name":"bk_2025_1226_1200"}
```

### BACKUP_LIST
```json
{"op":"BACKUP_LIST","token":"t_admin"}
```
Resp
```json
{"ok":true,"code":0,"msg":"OK","data":{"names":["bk_2025_1226_1200","bk_2025_1226_1500"]}}
```

### BACKUP_RESTORE
```json
{"op":"BACKUP_RESTORE","token":"t_admin","name":"bk_2025_1226_1200"}
```

---

## 业务层路径示例（可自定义但需一致）
- 论文：`/papers/<paperId>/<version>.pdf`
- 评审：`/reviews/<paperId>/<reviewer>.txt`
- 分配/决策：`/decisions/<paperId>/decision.txt`
具体权限与目录规划由 Server 权限模块实现，但必须仍通过上述文件接口访问 VFS。
