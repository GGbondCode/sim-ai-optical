# optical-reconfig

## 1. 这个模块是做什么的

`optical-reconfig` 是一个用于 ns-3 的轻量级光重构模块。

这一版 v1 的目标不是做真实光物理层仿真，而是在 ns-3 里先搭出一个“能跑通的闭环”：

- ring 拓扑正常运行
- 仿真过程中注入节点故障
- 系统收集当前集群状态
- 重构策略生成 `ReconfigPlan`
- 编排器在设定的重构延迟后执行重构
- 通过 `Ipv4::SetDown` / `Ipv4::SetUp` 切换候选链路
- 触发 IPv4 全局路由重算
- UDP 连续流切换到新的可达路径

也就是说，这一版重点解决的是：

- 控制面如何组织
- 数据面如何真正参与
- 重构动作如何在 ns-3 中落地

它适合作为后续扩展 GPU 故障、链路故障、更复杂拓扑、甚至更真实光层模型的基础骨架。

## 2. 当前 v1 已经实现了什么能力

当前已经支持：

- 通过 `FaultManager` 在运行时注入节点故障
- 维护节点状态、链路状态、ring 邻接关系等统一集群视图
- 在仿真开始前预注册 active 链路和 standby bypass 链路
- 通过 `RingBypassPolicy` 生成单次 ring bypass 重构方案
- 在故障发生时立即下线故障节点相关链路
- 在设定的 `reconfig delay` 后上线备用 bypass 链路
- 调用 `Ipv4GlobalRoutingHelper::RecomputeRoutingTables()` 触发路由重算
- 在 4 节点 ring 示例中用 UDP 连续流验证路径切换
- 通过测试验证策略输出与延迟生效逻辑

## 3. 当前 v1 不支持什么

当前限制如下：

- 只实现了节点级故障
- 只支持 IPv4 和 `Ipv4GlobalRoutingHelper`
- 只实现了 ring bypass 这一种策略
- 不包含真实光物理层行为
- 不在运行时动态创建设备或链路
- 所有候选 bypass 链路都必须在仿真开始前预先创建并注册

虽然 `FaultEvent` 里保留了 `GPU` 和 `LINK` 字段，但当前版本还没有实现这两种故障路径。

## 4. 端到端执行流程

本模块在运行时的完整闭环如下：

1. `FaultManager` 接收到故障注入请求。
2. `ReconfigOrchestrator` 收到故障事件并收集 `ClusterState`。
3. `RingBypassPolicy` 根据当前状态生成 `ReconfigPlan`。
4. `OpticalCircuitManager` 先立即下线故障节点相关链路。
5. `ReconfigOrchestrator` 等待设定的重构延迟。
6. `OpticalCircuitManager` 上线计划中的备用 bypass 链路。
7. `CommunicationTopologyManager` 应用新的 ring 邻接关系。
8. 调用 `Ipv4GlobalRoutingHelper::RecomputeRoutingTables()` 更新路由。
9. 正在发送的 UDP 流切换到新的路径继续发送。

v1 的关键设计点就是：

- 控制面可以看到故障、计划、执行和完成
- 数据面也会真实经历链路切换和路径改变

## 5. 当前目录结构有什么用

当前模块目录结构如下：

```text
optical-reconfig/
|-- CMakeLists.txt
|-- README.md
|-- helper/
|   `-- optical-reconfig-helper.{h,cc}
|-- model/
|   |-- optical-types.h
|   |-- fault-manager.{h,cc}
|   |-- communication-topology-manager.{h,cc}
|   |-- optical-circuit-manager.{h,cc}
|   |-- reconfig-policy.h
|   |-- ring-bypass-policy.{h,cc}
|   |-- reconfig-orchestrator.{h,cc}
|   |-- optical-switch-controller.{h,cc}
|-- examples/
|   |-- optical-ring-failure-demo.cc
|   `-- optical-reconfig-demo.cc
`-- test/
    |-- optical-reconfig-test-suite.cc
    `-- run-optical-reconfig-test-suite.sh
```

### `optical-types.h`

这是整个模块共享的数据类型定义文件，里面包括：

- `NodeState`
- `CircuitState`
- `RingNeighbor`
- `FaultEvent`
- `ClusterState`
- `ReconfigPlan`

它的作用是统一控制面数据模型，让“策略层”“状态层”“执行层”之间通过统一结构通信，而不是直接暴露 ns-3 的接口细节。

### `fault-manager.*`

故障管理层，负责：

- 接收故障注入请求
- 记录故障时间
- 把故障事件回调给 orchestrator

它本身不做重构决策，只负责“把故障送进去”。

### `communication-topology-manager.*`

拓扑状态层，负责：

- 保存 ring 顺序
- 保存每个节点的前驱/后继邻居
- 在重构后应用新的 ring 视图
- 输出可读的 ring 描述字符串，方便日志观察

它解决的是“逻辑拓扑长什么样”的问题。

### `optical-circuit-manager.*`

链路执行层，负责：

- 维护逻辑链路与真实接口句柄的映射
- 保存候选 optical circuit 的 active / backup 状态
- 调用 `Ipv4::SetDown` / `Ipv4::SetUp` 真正下线或上线接口
- 生成当前节点状态和电路状态给 orchestrator 使用

这个类是“控制面真正落到数据面”的关键位置。

### `reconfig-policy.h`

这是策略抽象接口。

它定义了一件事：

- 输入 `ClusterState + FaultEvent`
- 输出 `ReconfigPlan`

以后如果你要增加别的策略，不需要动 orchestrator，只需要实现新的 policy 类。

### `ring-bypass-policy.*`

这是 v1 默认实现的策略。

行为很明确：

- 如果节点 `X` 故障
- 找到 `prev(X)` 和 `next(X)`
- 如果存在预注册候选 bypass 链路，且两端端口可用
- 就生成 `prev(X) <-> next(X)` 的 bypass 重构计划

它适合第一版验证 ring 故障旁路逻辑。

### `reconfig-orchestrator.*`

这是当前模块真正的主入口。

它负责把各层串起来：

- 收到故障
- 生成集群状态
- 调用策略
- 执行链路切换
- 更新 ring 邻接
- 触发路由重算
- 输出日志

如果把整个模块看成一个系统，`ReconfigOrchestrator` 就是“总控”。

### `optical-reconfig-helper.*`

这是给外部场景代码使用的便捷入口。

它负责：

- 注册节点
- 设置 ring 顺序
- 注册候选链路
- 设置默认策略
- 设置重构延迟
- 返回一个安装好的 `ReconfigOrchestrator`

也就是说，写 example 或未来接到更大的仿真框架里时，一般最先接触的是这个 helper。

### `examples/optical-ring-failure-demo.cc`

这是当前 v1 的主要示例。

它做了这些事情：

- 创建 4 个节点的 ring
- 预装 active 链路和一个备用 bypass 链路
- 启动 UDP 连续流
- 在 `10 ms` 注入 `N1` 故障
- 观察链路切换和路径变化

它是验证“模块真正能不能跑起来”的最直接入口。

### `test/optical-reconfig-test-suite.cc`

这是模块测试文件，负责验证：

- policy 输出是否正确
- 不可用端口时是否会拒绝错误建链
- orchestrator 是否真的尊重重构延迟

### `model/optical-switch-controller.*`

这是旧架构遗留的占位文件。

当前它已经不再是有效入口，只是因为最初重构时工作区无法直接删除文件，所以保留为 deprecated placeholder。

当前真正入口已经是：

- `reconfig-orchestrator.*`

### `examples/optical-reconfig-demo.cc`

这也是旧示例遗留的占位文件。

当前实际有效的示例是：

- `optical-ring-failure-demo.cc`

## 6. 对外 API 应该怎么理解

这一版对外最重要的公开入口有：

- `ReconfigOrchestrator`
- `OpticalReconfigHelper`
- `ReconfigPolicy`
- `RingBypassPolicy`
- `FaultManager`
- `OpticalCircuitManager`
- `CommunicationTopologyManager`
- `optical-types.h` 里的公共数据结构

### `OpticalReconfigHelper`

这是最推荐的使用入口。

典型用法如下：

```cpp
OpticalReconfigHelper helper;
helper.RegisterNode (0);
helper.RegisterNode (1);
helper.RegisterNode (2);
helper.RegisterNode (3);
helper.SetRingOrder ({0, 1, 2, 3});
helper.SetReconfigDelay (MilliSeconds (5));

helper.RegisterCandidateCircuit (... active ring link ...);
helper.RegisterCandidateCircuit (... standby bypass link ...);

Ptr<ReconfigOrchestrator> orchestrator = helper.Install ();
orchestrator->InjectNodeFault (1);
```

### `ReconfigOrchestrator`

它是统一编排入口，主要职责是：

- 接收故障事件
- 组装 `ClusterState`
- 调用策略生成 `ReconfigPlan`
- 先执行故障隔离
- 延迟后执行 bypass 上线
- 更新逻辑 ring
- 重算路由
- 输出当前 ring 描述和最近一次 plan

### `OpticalCircuitManager`

这是 v1 中最关键的“数据面适配层”。

当前设计有一条重要原则：

- `ReconfigPlan` 只保留逻辑层信息
- `Ptr<Ipv4>` 和接口索引这些 ns-3 细节不暴露到公共 plan 中
- 真实接口映射只存在 `OpticalCircuitManager` 内部

这样可以让策略逻辑和 ns-3 设备实现解耦。

## 7. 这次我到底做了哪些工作

相对原本非常薄的 `OpticalSwitchController` 风格入口，这次重构主要做了这些事：

- 用 `ReconfigOrchestrator` 替换旧的 controller 入口思路
- 新增统一状态模型 `optical-types.h`
- 新增 `FaultManager`
- 新增 `CommunicationTopologyManager`
- 新增 `OpticalCircuitManager`
- 增加策略抽象接口 `ReconfigPolicy`
- 实现默认策略 `RingBypassPolicy`
- 重写 `OpticalReconfigHelper`，支持注册节点、ring 顺序、候选链路、策略和延迟
- 新增真实可运行的 4 节点 ring 故障示例
- 新增模块测试文件
- 更新 `CMakeLists.txt`，让新模型、示例和测试可以被编译
- 保留旧文件为占位，避免误删失败导致构建出错

一句话总结：

我把原来“只有一个很薄控制入口”的模块，扩成了一个完整的 v1 光重构框架，包含状态层、策略层、执行层、示例和测试。

## 8. 这个 demo 场景具体是什么

当前主示例是一个固定 4 节点 ring：

```text
N0 -> N1 -> N2 -> N3 -> N0
```

初始 active optical link：

- `N0-N1`
- `N1-N2`
- `N2-N3`
- `N3-N0`

预置 dormant bypass link：

- `N0-N2`

运行过程：

- `N0` 向 `N3` 持续发送 UDP 流
- 在 `10 ms` 注入 `N1` 节点故障
- 立即下线 `N0-N1` 和 `N1-N2`
- 在重构延迟结束后上线 `N0-N2`
- 重算路由
- UDP 流切换到新的路径继续发送

这个 demo 还会输出路由快照文件：

- `optical-ring-failure-demo.routes`

这个文件适合用来查看故障前、重构中、重构后的路由表变化。

## 9. 如何编译和运行

下面的命令都假设你当前目录在：

```bash
cd /home/jy/SimAI/ns-3-alibabacloud/simulation
```

### 9.1 配置

```bash
./ns3 configure --enable-examples --enable-tests
```

### 9.2 编译 demo

```bash
./ns3 build -j 8 optical-ring-failure-demo
```

### 9.3 运行 demo

```bash
./build/src/optical-reconfig/ns3.36.1-optical-ring-failure-demo-default
```

你会看到类似输出：

```text
[Demo] Initial ring: N0 -> N1 -> N2 -> N3 -> N0
[OpticalReconfig] Fault injected at 10 ms: node N1
[OpticalReconfig] Plan: remove=[N0-N1, N1-N2], add=[N0-N2]
[OpticalReconfig] Link added at 15 ms: N0-N2
[OpticalReconfig] Ring after reconfiguration: N0 -> N2 -> N3 -> N0
```

你还会看到 UDP sink 的输出：

- 重构前，数据从原路径到达
- 重构后，接收来源地址发生变化

这就是最直观的数据面切换证据。

### 9.4 查看路由快照

```bash
cat optical-ring-failure-demo.routes
```

### 9.5 用 CTest 跑 demo

```bash
ctest --test-dir cmake-cache -R ctest-optical-ring-failure-demo --output-on-failure
```

## 10. 如何运行模块测试

当前推荐的标准测试命令是：

```bash
./ns3 build -j 8 test-runner
./ns3 run "test-runner --suite=optical-reconfig --verbose"
```

如果你更习惯使用 `test.py`，现在也可以直接运行：

```bash
python3 test.py -n -s optical-reconfig -v
```

为了解决这个问题，我额外提供了一个模块内辅助脚本：

```bash
bash src/optical-reconfig/test/run-optical-reconfig-test-suite.sh
```

这个脚本会自动完成：

- 配置工程
- 构建标准 `test-runner`
- 调用 `test-runner` 只运行 `optical-reconfig` 这一组测试

成功时你会看到类似输出：

```text
PASS optical-reconfig
  PASS RingBypassPolicy should produce the expected bypass plan
  PASS RingBypassPolicy should not create a bypass when a port is unavailable
  PASS ReconfigOrchestrator should respect reconfiguration delay
```

## 11. 当前测试都在验证什么

当前测试主要覆盖三件事：

- `RingBypassPolicy` 在正常条件下是否生成正确的 bypass plan
- 当端口不可用时，是否会拒绝生成错误的 bypass plan
- `ReconfigOrchestrator` 是否真的遵守 `reconfig delay`，而不是在故障瞬间立刻上线 bypass

这三项覆盖了 v1 最关键的正确性要求。

## 12. 后续你可以怎么扩展

当前结构是为了后续扩展预留出来的。比较自然的演进方向有：

- 新增别的 `ReconfigPolicy` 实现
- 把 `FaultEvent` 扩展到 `GPU` 和 `LINK`
- 支持比 ring 更复杂的拓扑管理方式
- 把简单的 `SetDown/SetUp` 切换升级成更真实的 optical switching 行为
- 再往上和 SimAI 的更高层逻辑集成

当前架构最大的价值就是把：

- 策略
- 状态
- 执行

这三层分开了，后面不会全部耦合在一个类里。

## 13. 当前已知问题

目前要特别注意这些点：

- 这个 fork 现在已经补回了标准 `test-runner` 构建入口，因此可以直接使用标准 ns-3 测试命令
- `point-to-point` 模块的链接依赖也已经补齐，避免了全量测试/示例构建时常见的一类链接失败
- `optical-switch-controller.*` 和 `optical-reconfig-demo.cc` 现在只是兼容占位，不是有效实现

## 14. 最后用一句话总结这版模块

如果只记住一句话，那么可以这样理解：

这一版 `optical-reconfig` 已经把“故障注入 -> 计划生成 -> 链路切换 -> 路由更新 -> UDP 流切换”这条最核心的光重构闭环，在 ns-3 中真正跑通了。
