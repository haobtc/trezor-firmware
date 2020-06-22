## **概述**
  本文档的内容是使用BixinKEY测试第三方App的方法和结果。  
  分为两部分：
  - trezor官网验证过的APP(参见https://wiki.trezor.io/Apps),这部分会挑常用的、流程复杂的详细描述，其他的只给链接，不做详细介绍
  - 官网中没提到的目前流行的支持TREZOR的APP


## <br/>**TREZOR官网支持的APP**</br> 
###  <br/>**Trezor网页版APP**</br>
#### &nbsp;环境搭建
  &nbsp;注意 本文概述的步骤针对在运行Windows，macOS或Linux的计算机上使用Chrome或Firefox浏览器
  - 安装bridge插件(https://wallet.trezor.io/#/bridge)  注意在Chrome中，您也可以使用WebUSB连接设备，而在Firefox中，Bridge是唯一的选择
  - 将BixinKEY插入PC，并打开trezor网址(https://trezor.io/), 点击“I already own Trezor”按钮，选择你的设备型号，连接进入BixinKEY
  - Passphrase密语设置，BixinKEY默认设置密语，如果不使用直接点击Enter跳过

#### &nbsp; 创建一个钱包
  - 未激活的BixinKEY 点击“Create wallet” 创建一个新的钱包，会生成Account信息  
  - 已激活的BixinKEY 进去以后就有对应的Account信息

#### &nbsp; 发币
  
  - 点击左侧任意一个Account
  - 在上方的菜单中点击“接收”菜单，查看完整地址并在BixinKEY上再次检查及确认
  - 向该地址发送比特币，收到钱之后，账户的余额会增加，可以发币
  - 在上方的菜单中点击“发送”菜单，输入地址金额并选择费率，点击发送按钮
  - 在BixinKEY上确认交易信息并签名，收币地址收到测试币，发币成功

#### &nbsp; 结果
  - 可以使用BixinKEY
 

### <br/>**Electrum客户端**</br>
#### &nbsp; 环境搭建
  #### &nbsp; LINUX
  ```
    wget https://download.electrum.org/3.3.8/Electrum-3.3.8.tar.gz
    tar -xvf Electrum-3.3.8.tar.gz
    sudo apt-get install libusb-1.0-0-dev libudev-dev
    python3 -m pip install trezor[hidapi]
    python3 Electrum-3.3.8/run_electrum
  ```
#### &nbsp; 创建一个钱包
  - 根据Create New Wallet的索引创建一个硬件钱包，大概流程如下：<br/>
    **Create New Wallet-->Standard wallet-->Use a Hardware device-->Enter Passphrase(不想输入密语，直接pass)-->选择一个钱包地址类型，默认P2WPKH-->next-->next**
 
#### &nbsp; 发币
  - 点击Send按钮，输入Pay to(输出地址)和Amount(发送金额)，点击pay</br>
    <br/>![send](./pictures/electrum-send.png)</br>
  - 拖拽选择fee，点击Finalize(完成)，进入签名页面</br>
    <br/>![finalize](./pictures/electrum-finalize.png)</br>
  - 点击签名，此时需要在BixinKEY上点击确认按钮</br>
  - 点击broadcast按钮，广播交易</br>
    <br/>![broadcast](./pictures/electrum-broadcast.png)</br>
  - 回到历史记录页面可以看到未确认的交易</br>
    <br/>![unconfirmed](./pictures/electrum-unconfirmed.png)</br>
#### &nbsp; 结果
  - 可以使用BixinKEY

### <br/>**Mycelium**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:Mycelium</font>  

  
### <br/>**Exodus**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:Exodus</font>    


### <br/>**Electrum-DASH**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:Electrum-DASH</font>   


### <br/>**Electrum-LTC**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:Electrum-LTC</font>   


### <br/>**Etherwall**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:Etherwall</font>    


### <br/>**MetaMask**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:MetaMask</font>    


### <br/>**Electrum-DASH**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:Electrum-DASH</font>    


### <br/>**MyCrypto**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:MyCrypto</font>    


### <br/>**MyEtherWallet**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:MyEtherWallet</font>    


### <br/>**Nano Wallet**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:Nano_Wallet</font>    


### <br/>**Walleth**</br>
### &emsp; <font face="黑体" size=3>参考使用地址：https://wiki.trezor.io/Apps:Walleth</font>    


## <br/>**市面流行的支持BixinKEY的APP**</br>
### <br/>**Bitcoin core + Specter-Desktop**</br>
#### &nbsp;环境搭建
#### &nbsp;LINUX
#### &nbsp;搭建regtest私有链:
  - git clone https://github.com/bitcoin/bitcoin.git
  - cd bitcoin
  - ./autogen.sh
  - ./configure
  - make
  - 修改配置文件
    ```
      server=1
      txindex=1
      rpcuser=bitcoinrpc
      rpcpassword=bitcoinrpc
      rpcallowip=192.168.255.144
      regtest=1
      [regtest]
      rpcport=18443
    ```
  - src/bitcoind -daemon


#### &nbsp;搭建跟HW通讯的可视化GUI:
  - 安装Specter-Desktop(Specter-Desktop是专门为了方便bitcoin和硬件钱包交互设计的一个GUI，参考：https://github.com/cryptoadvance/specter-desktop)  
    ```
      python3 -m pip install cryptoadvance.specter
      python3 -m cryptoadvance.specter server
    ```  
  - 在浏览器中打开网址：http://127.0.0.1:25441/，可以进入UI界面，连接BixinKEY创建钱包及收币
#### &nbsp;创建一个钱包
  - 点击左侧"Add new wallet"，选择创建个人钱包还是多签钱包(个人钱包目前测试没问题，多签创建不成功)
    <br/>![create-wallet](./pictures/bitcoin-new-wallet.png)</br>
  - 选择钱包类型(Nested Segwit还是Segwit)，选择“trezor设备”，点击continue进入钱包  
    <br/>![select-wallet-type](./pictures/bitcoin-new-wallet-type.png)</br>
#### &nbsp;发币
  - 点击左侧账户，再点击send，填写地址和金额
    <br/>![send](./pictures/bitcoin-send.png)</br>
  - 最后点击trezor按钮，确定用trezor来签名此次交易  
    </br>![send](./pictures/bitcoin-trezor-sign.png)</br>
  - 在BixinKEY上确认交易，如设置PIN则会有弹窗要求输入PIN
  - 广播交易
    <br/>![send](./pictures/bitcoin-send-tx.png)</br>
  - 在收币地址收币成功
    <br/>![send](./pictures/bitcoin-uncofirm-tx-info.png)</br>
#### &nbsp;结果
  - 可以使用BixinKEY

### <br/>**bitcoin core+HWI**</br>  
#### &nbsp;环境搭建
#### &nbsp;LINUX
  &nbsp;参考网址：https://github.com/bitcoin-core/HWI/blob/master/docs/bitcoin-core-usage.md  
#### 搭建regtest私有链：
  - 同"Bitcoin core + Specter-Desktop"中搭建流程一致  
  
#### 搭建HWI运行环境：
  - sudo apt install libusb-1.0-0-dev libudev-dev python3-dev
  - git clone https://github.com/bitcoin-core/HWI.git
  - cd HWI
  - pip3 install -U setuptools
  - python3 setup.py install
  
#### &nbsp; 查找硬件
  - ./hwi.py enumerate (查找硬件)
  - ./hwi.py --device-type trezor --testnet getkeypool --wpkh 0 1000 (生成key) 
#### &nbsp; 创建一个钱包
  - bitcoin-cli -regtest createwallet "trezor0" true (创建钱包)
  - bitcoin-cli -regtest -rpcwallet='trezor0' importmulti 'keypool'(keypool为查找硬件中第二步的返回值)

    <br/>此图为查找硬件和创建钱包的具体实例：</br>
        ![create](./pictures/hwi-enumerate-getkeypool-createwallet-import.png)</br>
#### &nbsp; 发币
  - bitcoin-cli -regtest -rpcwallet=trezor0 walletcreatefundedpsbt '[]' '[{"2NDnUB67K9mdx2vyqaaf2YbmXaMtoDdfWFc":1}]' 0 '{"includeWatching":true}' true (创建交易，返回psbt1)  
  - bitcoin-cli -regtest decodepsbt pbst1 (查看交易详情)

    <br/>![create-tx](./pictures/hwi-create-tx-and-decode.png)</br>

  - ./hwi.py --device-type trezor --testnet signtx pbst1 (签名交易)
  - bitcoin-cli -regtest finalizepsbt psbt1 (删除一些数据，生成可广播的十六进制的rawtx)
  - bitcoin-cli -regtest sendrawtransaction rawtx(rawtx 是上一步返回的hex后对应的数据,返回一个tx_hash)
    <br/>![sign-tx](./pictures/hwi-sign-send-tx.png)</br>
#### &nbsp; 结果
  - 查看结果方法：
    ```
      bitcoin-cli -regtest -rpcwallet=receive_wallet listreceivedbyaddress (查看接受地址钱包的地址列表，可以看到上一步广播后返回的交易tx_hash)
    ```  
    <br/>![result](./pictures/hwi-result-6.png)</br>
  - 可以使用BixinKEY


## <br/>**Wasabi**</br>
#### &nbsp;环境搭建
#### &nbsp;LINUX
- APP下载地址：https://wasabiwallet.io/#download
- github地址：https://github.com/zkSNACKs/WalletWasabi
#### &nbsp; 创建一个钱包
- 安装成功后，插入BixinKEY，点击左侧Hardware wallet，再点击下方的search hardware wallet，会自动搜索到BixinKEY，并创建钱包
  ![load-wallet](./pictures/wasabi-load_wallet.png)</br>
#### &nbsp;发币
- 进入菜单tools/settings，修改网络为testnet，重启软件，点击receive菜单项，获取地址并向该地址发送测试币(此处地址生成要根据已有的钱包生成，我输入的Observer为Electrum)
  ![receive-addr](./pictures/wasabi-receive-address.png)</br>
- 点击send菜单项，输入金额和地址，点击Send Transaction
  ![send](./pictures/wasabi-send.png)</br>
- 此时会提醒你等待硬件确认，在BixinKEY上点击确认以后，会自动发送交易
  ![sign](./pictures/wasabi-send-transaction.png)</br>
- 回到交易历史页面会看到交易详情
  ![unconfirm](./pictures/wasabi-history.png)</br>
#### &nbsp;结果
- 可以使用BixinKEY

