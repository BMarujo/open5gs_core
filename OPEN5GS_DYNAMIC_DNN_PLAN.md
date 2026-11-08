# Open5GS: descoberta dinâmica de DNNs das UPFs

Plano de desenvolvimento e prompts sequenciais para Codex no VS Code.

Preparado em 5 de outubro de 2026; atualizado após as etapas autorizadas abaixo.
Este documento contém o plano e o estado das entregas, sem certificação de conformidade.

## Entrega atual — comparação de desempenho e publicação, 8 de novembro de 2026

Follow-up autorizado concluído: código atualizado em `384a54f4d9a`, commit/push,
imagem pública `bmarujo/open5gs:dnn-384a54f4d9a0`, digest
`sha256:a05e25b5cc4e39defa42b87fed82bcd0ace632ce477a53059f6ebee6fdffdb8b`.
Aplicada à SMF/UPF no Containernet e no cluster físico; as restantes nove NFs
mantêm Gradiant 2.8.0. Ambas as instalações passaram NAS, seleção learned-DNN
e tráfego UE; os 11 Pods ativos de NFs estão Ready em cada instalação.

Comparação isolada com a base exata `2c80f8f2ba9d`: PASS, cinco pares alternados,
dez sessões, quarenta medições TCP/UDP UL/DL. AMBR/MBR, 5QI, ARP e QFI iguais;
sem novo limitador de bitrate no diff auditado e sem regressão material de
throughput detetada. A rede de teste tem acesso de 50 Mbit/s: não é certificação
de capacidade máxima, carga multi-UE ou ausência absoluta de bugs.

Dois smokes físicos falharam por endpoints/caches antigos de Pods em nós já
indisponíveis. Guard operacional nos Services SBI NRF/SMF/BSF e refresh de runtime
resolveram o terceiro smoke; falhas preservadas. Não foram apagados à força os
Pods órfãos. Novos upgrades Helm exigem verificar esse guard enquanto persistir
o problema dos nós. Recursos temporários e subscritores sintéticos removidos.

[Relatório em inglês, resultados, imagens, evidências e rollback](docs/design/dynamic-dnn-performance.md).
As secções abaixo preservam o estado histórico de cada etapa; a publicação e o
rollout antes pendentes estão concluídos. A etapa 4B continua excluída.

## Estado histórico — Prompt 7, 8 de novembro de 2026

MVP das etapas 1–7 concluído para o âmbito documentado: anúncio de pools/DNNs
por PFCP, aprendizagem owned e utilização pelo seletor legacy. A etapa 4B
permanece excluída. A pedido do utilizador, não existe entrega de PR/rascunho
upstream; o fecho inclui um mapa interno coerente das alterações.

Revisão contra `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`, mantendo `main`,
HEAD `78f91ae6e5674503c766991e0d8cd10c515869b6` e versão declarada 2.8.0.
Preservadas as alterações locais herdadas. A revisão corrigiu validação demasiado
restritiva de IEs extensíveis 177/258/60/96 segundo TS 29.244 V19.5.0 §8.1.2 e
as figuras individuais; IEs fixos continuam com comprimento exato. Corrigida
também a captura dos harnesses em diretórios privados, sem relaxar permissões.

PASS: compilação completa isolada; core/crypt/unit/smf-selection; 9 casos codec,
10 associação e 6 seletor (incluindo 6 144 comparações legacy); ASan/UBSan sem
supressões nas suites dirigidas; regeneração fresh/cached e 28 caches idênticos;
7 cenários de associação, 15 de configuração e 14 de seleção; NAS/N4/GTP-U com
ambos os iniciadores e retorno C/B/C/B; perda assimétrica, perda simultânea e
perda breve de Heartbeat; regressões 5GC/EPC; imagens locais e smoke PFCP.
Os bytes independentes da correção são descodificados por TShark 4.2.2.

O fallback legacy DNN OU localização e os fallbacks existentes permanecem.
O erro de rejeição Session Establishment já presente na base continua registado
como FAIL protocolar, com no-match caracterizado separadamente dos positivos.
Não há falhas finais abertas nas verificações executadas da implementação nova.

A imagem pública e os testes anteriores do cluster/Containernet são evidência
do MVP antes da última correção de extensões. A correção final tem imagens locais
e novos testes isolados; **publicação e rollout dessa revisão não foram feitos**.
Não houve novo deployment, commit, push ou PR nesta etapa. IPv6/slices NAS/N6,
outros vendors, carga/soak e a campanha no worker físico indisponível continuam
NOT RUN; 4B é NOT RUN (optional). Não é alegada conformidade integral Release 19.

Documentação canónica em inglês:

- [Guia de configuração e operação](docs/design/dynamic-dnn.md).
- [Revisão final, mapa de alterações, evidências e limitações](docs/design/dynamic-dnn-final-review.md).
- [Matriz cláusula → comportamento → teste → evidência](docs/design/dynamic-dnn-conformance-matrix.md).

As secções cronológicas seguintes preservam o estado na data de cada etapa;
os respetivos bloqueios/NOT RUN antigos não substituem este estado final.

### Validação local adicional IPv6/slices — 8 de novembro de 2026

A pedido do utilizador, executada sem aceder ao Kubernetes. PASS: testes existentes
de registo dual stack, slices same/different DNN e EPC IPv6; funcionalidade off
e aprendizagem ativa com ambos os iniciadores. Dez invocações / treze casos ABTS.
PASS adicional: seis sessões novas (IPv6-only, dual stack e IPv4, por iniciador),
primeiro N4 correto e zero retransmissões, doze pares ICMPv6 por GTP-U e anúncios
de família descodificados independentemente. Não foi necessário corrigir o core;
hash mantém-se `d8e528dfc3ab…`. Só foram acrescentados testes/documentação.

O NOT RUN anterior de IPv6 NAS/data fica superado para este laboratório local.
Permanecem sem teste DN IPv6 externo, transportes N4/N3 IPv6, NAS com anúncios
PFCP restritos por slice e estes cenários no cluster. A política de slices e o
fallback legacy não foram alterados. [Relatório e reprodução em inglês](docs/design/dynamic-dnn-ipv6-slices.md).

## Estado após a auditoria de 6 de outubro de 2026

Revisão normativa em 7 de outubro de 2026: o contrato usa agora **Release 19,
TS 23.501 V19.5.0 e TS 29.244 V19.5.0**, alinhadas com a fonte do gerador PFCP.
As referências Release 17 da primeira auditoria foram substituídas após revisão
das cláusulas. Continua a ser documentação da etapa 1, sem implementação.

A etapa 1 foi executada no HEAD `2c80f8f2ba9d378bc845d73fe2fbf4820039fc41`, branch `main`, versão declarada `2.8.0`, sem tag local no HEAD. O commit de referência abaixo é um antepassado, 205 commits atrás. Não houve mudança de versão nem alteração do comportamento do core. O plano era a única alteração local inicial (ficheiro não versionado) e foi preservado antes da revisão.

Documentação em inglês:

- [Auditoria, baseline, evidências e estado de trabalho](docs/design/dynamic-dnn-audit.md).
- [ADR-001: contrato e mapa de alterações](docs/design/ADR-001-dynamic-dnn-learning.md).
- [Experiências reproduzíveis do seletor e contrato de identidade](docs/design/dynamic-dnn-audit-experiments.md).
- [Revisão Release 19, condições, erros e testes adicionais](docs/design/dynamic-dnn-release19-review.md).
- [Etapa 2: codec, diff, fixtures e resultados executados](docs/design/dynamic-dnn-codec.md).
- [Etapa 3: anúncio, associação, ownership e evidência executada](docs/design/dynamic-dnn-association.md).
- [Etapa 4A: seleção legacy com static/learned e evidência executada](docs/design/dynamic-dnn-selection.md).
- [Etapa 5: integração local, imagens, resultados e limitações](docs/design/dynamic-dnn-integration.md).
- [Runbook: reprodução local, fixtures isoladas para cluster e rollback](docs/design/dynamic-dnn-runbook.md).
- [Correção de 4 de novembro: mappings após restart/Heartbeat e novas evidências](docs/design/dynamic-dnn-restart-fix.md).
- [Recuperação após perda assimétrica de Heartbeats e ownership de eventos](docs/design/dynamic-dnn-pfcp-recovery.md).
- [Etapa 6: imagem pública, Helm e validação real no cluster](docs/design/dynamic-dnn-cluster.md).
- [Alinhamento das restantes NFs com Gradiant 2.8.0 e validação de 5 de novembro](docs/design/dynamic-dnn-version-alignment.md).
- [Prompt 6: matriz isolada Containernet/K3s e retorno N6, 8 de novembro](docs/design/dynamic-dnn-containernet.md).

A regeneração da etapa 1 com caches existentes e vazios passou, reproduzindo o codec original (incluindo os bugs). A caracterização de duas funções extraídas do seletor passou em 8 asserts. O modelo de identidade passou entre processos novos; o teste de reinício real da UPF fica para a etapa 3. Nessa auditoria, a baseline completa Meson ficou bloqueada por `talloc` ausente; esse impedimento foi superado no ambiente isolado da etapa 2. Estes resultados da etapa 1 não validavam uma feature implementada.

O ADR é o contrato para os prompts seguintes. As opções de anúncio/aprendizagem
foram implementadas na etapa 3; a etapa 4A liga static/learned ao seletor legacy.

### Estado da etapa 2 — codec PFCP, 7 de outubro de 2026

A etapa 2 foi autorizada e concluída; não foi iniciado anúncio, aprendizagem ou
seleção. Corrigidos gerador, caches e ficheiros gerados para 233/177 e repetições
por contexto. O adapter PFCP distingue defeitos semânticos opcionais, capacidade
local não suportada e framing inválido, sem alterar o parser partilhado GTP/core.
O par de identidades do Create PDR e o singleton de Usage Information também
foram corrigidos como efeitos do mesmo alias.

PASS: baseline original e targets alterados `core`, `unit`, SMF, UPF, SGW-C e
SGW-U; testes `core`/`unit`, oito casos ABTS novos com subcasos, máximos simultâneos
16/16/8, ASan/UBSan com deteção de fugas, geração fresh/cached idêntica e fixtures
independentes descodificadas por TShark 4.2.2. A nota de features futuras não
descodificadas por esse decoder limita a evidência ao codec alvo. Não houve
deployment ou execução de NFs.

NOT RUN: procedimentos/handlers/FSM de T18/T25, condições/defaults de T27 que
dependem de handlers, reinícios reais T24, aprendizagem/seleção e tráfego N6.
Ver relatório da etapa 2 para falhas intermédias corrigidas, comandos e hashes.
A etapa 3 exige novo pedido; não se avança automaticamente.

### Estado da etapa 3 — anúncio e aprendizagem, 8 de outubro de 2026

Implementados `upf.pfcp.advertise_ue_ip_pools` e `smf.pfcp.learn_upf_dnn`, ambos
`false` por omissão, e `upf.session[].pool_id` explícito/persistente. Setup Request
e Response anunciam os subnets com DNN explícito, com V4=`0x01` e V6=`0x02`
(R19 §8.2.177). A SMF publica snapshots owned separados de `node->dnn[]`, incluindo
identidades opacas, DNN, família e slices. Seleção/allocator permanecem inalterados.

Validação wire precede lookup/merge e Request Accepted. Cause, send/commit e
resultados dos handlers condicionam a transição inicial. Erros opcionais não são
rejeições automáticas da associação; capacidades inválidas/não suportadas não
conservam listas antigas. Respostas erradas correlacionadas terminam a transação
e notificam o callback da aplicação. Epoch local impede publicação por respostas
obsoletas. Cleanup ocorre em substituição, timeout, Heartbeat restart e destruição.

PASS: compilação dos quatro NFs, `core`/`unit`, sete casos ABTS de associação com
subcasos (mais os oito do codec), ASan/UBSan com leak detection, quinze cenários
de configuração UPF, sete cenários com executáveis reais isolados, ambos os
iniciadores, reinício UPF mantendo a SMF, T24 e descodificação externa TShark 4.2.2.
A verificação externa apanhou uma inversão inicial V4/V6 no sender, corrigida e
retestada; o relatório regista também as falhas intermédias do harness.

Limitações observadas: SQN reutilizado dentro da janela de retenção é tratado
como retransmissão; Heartbeat pode invalidar um snapshot acabado de publicar.
No caminho passivo de restoration não se adicionou novo Setup automático, para
preservar a lógica de recuperação de sessões. Um Setup posterior reaprende os
metadados. NULL após invalidação não equivale a ABSENT num Setup aceite; 4A tem
de respeitar esta diferença. Não se certificou retenção de sessões do mesmo peer.

NOT RUN: seleção dinâmica/4A, política estrita 4B, sessões UE/N6, cluster e
Association Update completo. O relatório inglês contém comandos, evidência e
critério de avanço. Etapa 3 concluída para este âmbito; aguarda pedido explícito
para 4A. Não se avança automaticamente.

### Estado da etapa 4A — seleção legacy, 11 de outubro de 2026

O braço DNN do seletor SMF consulta o snapshot owned sem alterar `node->dnn[]`.
Feature off usa static; aprendizagem ativa usa static apenas para ABSENT aceite.
NULL invalidado e anúncios inutilizáveis/unsupported não recuperam static. Grupos
utilizáveis respeitam família IP e conservadoramente excluem restrições de slice;
famílias explícitas de grupos do mesmo DNN podem cobrir dual stack em conjunto.
Família ausente permanece desconhecida, com matching semelhante ao mapping static.

Mantidos DNN OU localização, ordem/rotação, `no_pfcp_rr_select`, RR associado e
fallback final para o primeiro peer, mesmo indisponível. Logs identificam DNN,
origem, razão, estado de associação, geração e peer. Allocator, callers e lógica
de recuperação/libertação de sessões não foram alterados.

PASS: baseline herdada e compilação final dos quatro NFs; `core`, `unit` e a nova
suite SMF ligada ao código real (3/3), seis casos ABTS incluindo 6 144 combinações
comparadas com a política anterior; ASan/UBSan com leak detection; 14 cenários
isolados, 32 seleções, ambos os iniciadores, precedência/reassociação, perda e
retorno após Setup explícito, IPv4/IPv6/dual stack, localização e fallbacks. A
primeira sessão alocada para edge1 escolheu B por learned/DNN, sem `client.upf`
nem localização; a SMF manteve o PID. Adição de C produziu B/C/B e preservou o
owner/endereço da sessão alocada retida. TShark 4.2.2 confirmou os anúncios A/B/C.

Limite da evidência: o driver exclusivo de teste injeta sessões alocadas na
fronteira do seletor, no poll thread da SMF real; não executa NAS/UDM/PCF nem
Session Establishment PFCP. Continuidade de sessões UE estabelecidas, retorno
N6, pools incompatíveis no plano de dados e cluster são NOT RUN. No S5-C o
caller pode selecionar antes de definir a família IP (tipo 0); o braço learned
não produz match nesse caso, mantendo localização/fallback e a ordem existente.
Os limites de cache/Heartbeat da etapa 3 continuam aplicáveis.

Falhas intermédias de compilação e de configuração/recuperação do harness foram
corrigidas/documentadas; nenhum FAIL final pendente. Etapa 4A concluída para este
âmbito. Nessa data, 4B e etapas seguintes não tinham sido executadas nem autorizadas.

### Estado da etapa 5 — integração e imagens, 11–12 de outubro de 2026

Executada apenas a etapa 5 autorizada para o MVP 4A. HEAD, branch e versão
mantidos; alterações herdadas preservadas. Novos testes, packaging e documentação
não alteram o comportamento do core. Referências mantidas em Release 19 V19.5.0.

PASS: build completo fresh isolado (4 707 passos), `core`/`crypt`/`unit`/
`smf-selection`, regressões de associação (7 cenários), configuração (15),
seletor (14) e memória dirigida ASan/UBSan. Testes existentes feature-off de
registration/auth-test, slice/different-dnn-test e attach/simple-test passaram.
UE sintético real por NAS/NGAP estabelece sessões sem mappings por peer nem
matches de localização: primeiros pedidos N4 vão para A/internet, B/edge1,
C/edge1, B/edge1, C/edge1, sem retries nesses cinco pedidos. GTP-U/ICMP validado
por conteúdo/TEIDs e TShark 4.2.2; sessão de A mantém dados após adição de B/C.
Caso secundário CP-iniciado comprova precedência learned sobre static conflituoso.

Fallback sem match continua possível e foi observado no primeiro pedido N4.
Pools CIDR incompatíveis para o mesmo DNN continuam indistinguíveis pelo anúncio:
C aceita endereço CP fora do seu subnet mas o retorno normal falha. PASS deste
teste significa caracterização negativa, não compatibilidade garantida.

FAIL: retorno normal de C publica learned e depois invalida-o no Heartbeat de
restart; pedido seguinte vai primeiro para B, não C esperado. Reproduz limite
documentado da etapa 3. FAIL baseline independente: resposta de erro Session
Establishment sem Node ID é descartada pela SMF, causando retransmissões e
timeout antes do NAS reject. Reprodutor wire mínimo dá bytes idênticos no HEAD
limpo e na versão alterada. Nenhuma destas falhas foi escondida com alteração
do parser, recuperação automática ou correção protocolar fora de âmbito.

Imagens locais construídas e arrancadas: SMF/UPF
`2c80f8f2ba9d-dirty-e62810f8908e`; IDs/digests, diff, dependências, comandos e
archives constam do relatório. Payload dos dois executáveis e 26 bibliotecas
built é idêntico ao build fresh. Não houve publicação/push/PR/deployment.

NOT RUN: T09/T10 **(optional)**, etapa 4B, cluster/API/CNI/admission, NAS completo
nas imagens runtime, novos cenários NAS learned IPv6/slices e N6 externo.
Fixtures render-only, recolha e rollback por UID preparados; cluster exige
novo pedido, contexto/namespace explícitos, NFs/subscribers/UE isolados e
permissões/rede verificadas. Evidência durável em
`/home/marujo/open5gs_stage5_results_20261011`. A entrega da etapa 5 termina
com os FAILs e impedimentos acima; não declara todos os gates fechados.
**Não avançar automaticamente para 4B, etapa 6 ou migração de sessões.**

### Correção autorizada — defeito introduzido na aprendizagem, 4 de novembro de 2026

Corrigida a limpeza que apagava o snapshot novo depois de a UPF regressar e
anunciar a DNN. O snapshot substituto é ligado ao Recovery Time Stamp apenas
por resposta a um Heartbeat enviado depois do Setup aceite, na mesma epoch.
Respostas a probes antigos não confirmam dados novos. Um reinício posterior
continua a invalidar metadados confirmados para a instância antiga; timeout,
desassociação e destruição continuam a libertá-los. O timestamp do Setup não
é usado para alterar o histórico de recuperação. Política legacy, allocator e
recuperação de sessões permanecem como estavam; sem renegociação adicional.

PASS: build completo, quatro suites Meson, associação/reassociação, seletor,
memória dirigida e regressões existentes 5GC/EPC. NAS/N4/GTP-U real confirma
perda e regresso de C, seguida por quatro sessões novas com primeiros pedidos
N4 em C/B/C/B, sem retries, nos dois iniciadores. Sessões existentes em A/B
continuam a passar dados e C mantém dados/mapping após o Heartbeat periódico
seguinte. Teste novo falha no core anterior e passa na correção.

Imagens locais atualizadas para `2c80f8f2ba9d-dirty-769ebcded9fe`; as anteriores
`dirty-e62810f8908e` não incluem a correção. Evidência em
`/home/marujo/open5gs_dnn_restart_results_20261104`. O FAIL histórico do retorno
fica substituído por estes resultados; Node ID nas respostas de erro e cache
SQN da baseline não foram corrigidos. Não se afirma ausência de outros bugs.
Cluster, 4B, publicação e alterações a deployments continuam fora deste pedido.

### Correção autorizada — recuperação PFCP assimétrica, 4 de novembro de 2026

Confirmado com SMF/UPF reais: perder apenas Heartbeat Responses numa direção
pode deixar um lado desassociado e o outro associado, sem reinício. Um peer
inicialmente passivo não voltava a iniciar Setup depois do timeout. O novo
pedido do utilizador autoriza corrigir este caso, incluindo a limitação anterior
de não adicionar renegociação. Peers com associação anterior passam a tentar
Setup automaticamente; peers passivos nunca associados continuam passivos.

CP recovery Setup pede retenção de todas as sessões com IE 183 vazio; a UPF
já associada pode confirmar PSREI no IE 184, conforme R19.5.0. Retenção seletiva
não é suportada e não é convertida em retain-all. Timestamp do Setup continua
a ser ignorado para deteção de restart. Timeout na SMF limpa learned; novo
Setup aceite reaprende. A lógica de falha/restoration de sessões, allocator e
seleção legacy permanecem. Não há promessa de preservar sessões no peer que
a SMF declarou falhado ou numa UPF cujo processo reiniciou.

A validação encontrou também um defeito introduzido nos caminhos de descarte
de Setup de UPF/SGW-C/SGW-U: eventos de pool eram libertados como heap.
Corrigidas as três funções de libertação; SMF já usava heap corretamente.
Testes incluem 128 respostas independentes sem transação por NF e recuperação
simultânea normal/ASan. Falhas anteriores e backtrace ficam preservados.

PASS: build completo, quatro suites Meson, associação/seletor, ASan/UBSan
dirigido, recuperação unilateral/bilateral, perda breve, feature-off, regressões
5GC/EPC e restart nos dois iniciadores. NAS/N4/GTP-U confirma novos pedidos
em C/B/C/B ou B/C/B/C após recuperar C, sem retries, preservando dados em A/B.
A rotação depende do cursor; C volta a ser elegível. TShark 4.2.2 confirma o anúncio.

FAIL excluídos: leak global de 96 bytes em talloc null-tracking, reproduzido
sem Open5GS e suprimido apenas por essa stack nas execuções instrumentadas
completas; Node ID no erro de sessão e cache SQN da baseline continuam intactos.
NOT RUN: 4B (optional), cluster, outros vendors, NAS completo nas imagens e
retenção seletiva. Evidência: `/home/marujo/open5gs_pfcp_recovery_results_20261104`;
imagens locais `2c80f8f2ba9d-dirty-0d6a194893d6`. Ver ADR Decisão 7 e relatório
para os limites e resultados reais. Sem deployments, publicação, push ou PR.

### Etapa 6 autorizada — publicação e validação Kubernetes, 4 de novembro de 2026

O utilizador autorizou explicitamente commit/push do core, publicação no Docker Hub
público e atualização das SMF/UPFs existentes. Código publicado em `main`; imagem
única `bmarujo/open5gs:dnn-055eb4a770d0`, compilada com todos os executáveis e
bibliotecas, compatível com o chart Gradiant **2.3.4** já instalado. Digest fixado
nos valores Helm de sete releases/oito Deployments; não se alterou o chart nem
as imagens das restantes NFs.

PASS: build completo/quatro suites, publicação pública e digest/imageIDs reais;
12 sessões positivas nos cenários dinâmicos mais uma após restaurar a configuração
normal, sempre com primeiro pedido N4 correto e sem retransmissões. IE 233 de B/C
confirmado por TShark 4.2.2, identidade de C persistente, rotação C/B/C/B e
perda/regresso automático sem reiniciar a SMF durante esses testes. Sessões em A/B
mantiveram dados. Caso sem DNN compatível manteve fallback legacy e rejeição NAS.

Falhas intermédias documentadas: libtins em falta na primeira imagem (corrigido
antes da publicação), encaminhamento SBI para a SMF antiga/pool de clientes SCP
esgotado (renovados SCP/AMF mantendo imagens), captura sem privilégios (corrigido
no harness). Recursos/pools temporários e 16 subscribers sintéticos foram limpos;
a configuração final normal voltou a passar NAS/N4/dados.

NOT RUN ambiental: SMF/UPFs edge-1 no worker já NotReady; templates foram atualizados,
mas não se mudou placement nem se forçou a limpeza dos Pods antigos. As quatro
workloads disponíveis SMF/UPF ficaram Ready. 4B continua NOT RUN (optional).
Relatório inglês e evidência em `open5gs_cluster_results_20261104T154429Z`.
Na data deste relatório, etapa 7 e reparação do worker não tinham sido iniciadas.
A etapa 7 foi entretanto concluída; publicação de PR foi retirada do âmbito.

### Prompt 6 concluído no Containernet/K3s — 8 de novembro de 2026

Executada a matriz dinâmica com as restantes NFs em Gradiant 2.8.0, num core
completo e MongoDB próprios em namespace dedicado. PASS: isolamento de descoberta,
IE 233 e aprendizagem sem mappings estáticos por peer, adição B/C com sessões em
A/B, rotação legacy, perda e regresso automático de C com pool ID persistente,
uso após Heartbeats e SMF com UID/PID constantes e zero reinícios. Doze sessões
positivas tiveram primeiro N4 correto e zero retransmissões. Cinquenta e sete
verificações de dados correlacionam GTP/TEIDs, saída N6 e respostas nas três DNs
reais da emulação; oito capturas têm zero drops. Não foi só echo ao gateway UPF.

O caso sem match preserva fallback/rejeição legacy; uma nova tentativa NAS
automática foi identificada separadamente dos retries PFCP. Fixtures e falhas
intermédias corrigidas ficaram preservadas. Cleanup por ownership/UID removeu o
laboratório e os sniffers. Depois foi instalada a configuração normal atualizada,
com Multus e 11 NFs Ready, sem alterar a árvore/cluster físico. A emulação e esse
core normal ficaram ativos. Os profiles edge/TAC4 foram adaptados e passaram
validação estática; não se alegam campanhas live desses profiles.

Relatório inglês acima; evidência em
`open5gs_containernet_dnn_results_20261108T073927Z/attempt-04/analysis-04`.
4B continua NOT RUN (optional); IPv6, roaming, carga e cenários além do MVP ficam
fora desta campanha. Não houve alteração de comportamento do core, commit/push,
publicação de nova imagem ou avanço automático para a etapa 7.

## 1. Resultado pretendido

A SMF mantém os serviços e os recursos de endereçamento configurados pelo operador. Uma UPF nova associa-se por PFCP e anuncia informação dos seus pools/DNNs. A SMF aprende em runtime os DNNs utilizáveis nessa UPF e fornece essa informação ao seletor existente, substituindo a necessidade de declarar previamente `client.upf[].address`/`dnn` para UPFs que iniciam a associação.

O MVP preserva a política atual de seleção e os seus fallbacks; apenas substitui a origem estática do mapping por metadados aprendidos na associação. Uma segunda fase opcional pode tornar a compatibilidade DNN uma condição estrita e impedir fallback para UPFs incompatíveis.

O operador deixa de ter de adicionar a cada nova UPF um mapping estático endereço N4 -> DNN no YAML da SMF. Continua a configurar os DNNs/pools usados neste MVP na SMF, os serviços autorizados no subscriber e o DNN pedido pelo UE. Isto é uma escolha de âmbito: não significa que o Open5GS exija sempre uma entrada explícita por DNN em todas as secções YAML; existem configurações genéricas.

Exemplo de aceitação:

- SMF configurada para `internet` e `edge1`.
- UPF A lançada com recursos de `internet`, iniciando PFCP para a SMF.
- UPF B lançada mais tarde com recursos de `edge1`, também iniciando PFCP.
- Sem mappings de DNN por peer na SMF e sem matches concorrentes de localização, pedidos `internet` usam A e pedidos `edge1` usam B, desde que os peers compatíveis estejam associados.
- Uma segunda UPF para `edge1` passa a ser candidata após associação válida.
- A SMF não reinicia para aprender estes peers e as suas capacidades.
- No MVP, a inexistência de uma candidata compatível conserva o fallback legacy do Open5GS; rejeição estrita é hardening opcional.

O estabelecimento da sessão pressupõe que já existe uma candidata pronta. Este trabalho não acrescenta espera pelo deployment, regras por SUPI, migração, ULCL, NRF discovery de UPFs, criação de DNNs/pools em runtime, controller Kubernetes ou algoritmo de placement.

## 2. Base técnica e fontes

Referência histórica do plano (não o HEAD auditado): Open5GS v2.8.0, commit `157f611a530e292e40ec50f9d23f0ef5d4fcd6a6`:

https://github.com/open5gs/open5gs/tree/157f611a530e292e40ec50f9d23f0ef5d4fcd6a6

O Codex deve identificar o HEAD real do fork. Não deve mudar silenciosamente de versão, aplicar números de linha antigos ou fazer downgrade para reproduzir este plano. Se o fork estiver mais adiantado, deve identificar as diferenças relevantes e adaptar o plano.

Fontes normativas fixadas, ambas Release 19 V19.5.0:

- S1: TS 23.501 V19.5.0, §6.3.3.1–§6.3.3.3: https://www.etsi.org/deliver/etsi_ts/123500_123599/123501/19.05.00_60/ts_123501v190500p.pdf
- S2: TS 29.244 V19.5.0: https://www.etsi.org/deliver/etsi_ts/129200_129299/129244/19.05.00_60/ts_129244v190500p.pdf
- S3: guia oficial de build, documentação viva, a confrontar com o commit: https://open5gs.org/open5gs/docs/guide/02-building-open5gs-from-sources/

V19.5.0 é o alvo reprodutível escolhido, sem alegar que é a revisão mais recente.
O gerador usa `lib/pfcp/support/r19.5.0/29244-j50.docx`; manter essa fonte e as
definições R19 não relacionadas. A versão Open5GS `2.8.0`, a Release 19 da 3GPP
e a versão `1` no header PFCP são identificadores distintos. Não mudar o header
para 19 nem deduzir conformidade integral do core a partir da versão do gerador.

### Conclusão normativa delimitada

S1 §6.3.3.2 inclui explicitamente associação N4 iniciada pela UPF como forma de a SMF conhecer uma UPF. Provisionamento e seleção por sessão são distintos; round-robin não é uma obrigação 3GPP. O MVP só fornece a origem aprendida do mapping DNN, sem implementar todos os requisitos de seleção R19, como funcionalidades UPF obrigatórias recebidas da UDM; o cenário controlado não pressupõe esses requisitos adicionais.

S2: o antigo §8.2.82 está Void. O candidato é UE IP Address Pool Information (233), opcional quando há pools configuradas: §6.2.6.3.1, tabelas 7.4.4.1-1, 7.4.4.1-3 e 7.4.4.2-1. O grupo exige Pool Identity e permite Network Instance. §5.21.1–§5.21.2 distingue alocação no CP/UP e o sinalizador UEIP. O uso escolhido para pools reais com allocator CP é uma interpretação fundamentada, não certificação. §7.2.3.2 e §7.6 exigem distinguir erros semânticos opcionais de erros obrigatórios e de comprimento; ver a classificação no ADR.

Em S2 R19, respostas erradas correlacionáveis com um pedido pendente exigem parar retransmissões e notificar a aplicação (§7.6.1/3/6), mesmo que sejam descartadas. UPF Instance ID é condicional quando disponível; a ausência por si só não prova essa disponibilidade remota. Outras condições e defaults opcionais, incluindo UN6DM/Packet Inspection, têm semântica própria: a precedência static/learned do MVP aplica-se apenas ao IE 233. Ver T25/T27 e a revisão R19.

A adoção deste mecanismo não prova conformidade integral. A evidência terá de cobrir a interpretação normativa, os bytes emitidos, o comportamento e as limitações. Não há certificação resultante desta revisão.

### Pontos reconfirmados no HEAD original (ver evidência detalhada na auditoria)

Estas constatações descrevem o baseline antes das alterações; a etapa 2 corrigiu
os pontos de codec/geração 5, 6 e 10 na working tree, conforme o relatório.
A etapa 3 implementou anúncio, metadados owned e validação/transições de associação
(pontos 1, 2, 3, 9 e 11); a ligação ao seletor continua por implementar.

1. `lib/pfcp/build.c`: `ogs_pfcp_up_build_association_setup_request()` e `ogs_pfcp_up_build_association_setup_response()` são os builders relevantes. O request não anuncia atualmente os DNNs configurados em `session`.
2. `lib/pfcp/handler.c`: `ogs_pfcp_cp_handle_association_setup_request()` e a variante `..._response()` não alimentam o seletor com esses DNNs.
3. `lib/pfcp/context.h`: `ogs_pfcp_node_t` contém `const char *dnn[]`; `lib/pfcp/context.c` preenche a lista com referências da configuração YAML. Novos dados de rede precisam de ownership próprio, não de uma cópia cega de ponteiros.
4. `src/smf/context.c`: `compare_ue_info()` aceita DNN OU localização. `selected_upf_node()` contém fallback para peers fora da correspondência. Preencher a lista não garante filtragem estrita.
5. `lib/pfcp/message.h` e `message.c`: há definições para o grupo 233, mas o campo correspondente na associação e o descritor da mensagem referenciam UE IP Address (93); dentro do grupo, Pool Identity também referencia o descritor errado. Confirmar no fork e testar os bytes: o nome do membro não prova o tipo transmitido.
6. Os ficheiros de mensagem são gerados. Investigar `lib/pfcp/support/pfcp-tlv.py`, os caches e a fonte usada na geração. A correção precisa de sobreviver à regeneração.
7. `src/smf/pfcp-sm.c` e `src/upf/pfcp-sm.c` tratam Setup/reassociação. Na referência inspecionada não há casos de Association Update equivalentes. Não assumir hot reload ou Update completos.
8. No caminho 5GC examinado, a atribuição de IP em `src/smf/nudm-handler.c` precede a seleção em `src/smf/npcf-handler.c`. O MVP não altera essa ordem.
9. `ogs_pfcp_subnet_t` e o parser atual de `session` contêm subnet, gateway, DNN, ranges e interface, mas não uma Pool Identity PFCP. O IE 233 exige o filho UE IP Address Pool Identity (177), pelo que o MVP precisa de acrescentar uma identidade explícita ao schema da UPF.
10. Os caches do gerador marcam os campos de Association como UE IP Address (93) e `tlv_more: 0`/`max_tlv_more: 0`, apesar de a norma permitir vários IE 233. A correção tem de abranger tipo, nesting e multiplicidade, não apenas renomear a struct gerada.
11. No estado inicial da SMF, `src/smf/pfcp-sm.c` chama os handlers de Association Setup e transita para `associated` sem usar o valor booleano devolvido. Uma associação rejeitada não pode transitar para `associated` nem publicar um snapshot novo. Isto não transforma o fallback legacy numa garantia de elegibilidade estrita. Os handlers CP/UP também ignoram Cause nas respostas e enviam Request Accepted antes da validação completa; os callers ignoram resultados na reassociação. Erro semântico num IE opcional não implica rejeição da associação.

## 3. Contrato funcional do MVP

Estas decisões foram validadas na auditoria e implementadas nas etapas 2–4A, com as correções posteriores do ADR. São políticas de implementação, não exigências adicionais da 3GPP. A secção 3.3 continua opcional e não implementada.

### 3.1 Anúncio e representação

- Trabalhar com pools reais configuradas na UPF e os respetivos DNNs explícitos.
- Validar o uso do IE 233 para este modelo antes de programar. Não o tratar como um contentor arbitrário de strings.
- Acrescentar o campo `upf.session[].pool_id`: string explícita de 1–64 bytes ASCII `[A-Za-z0-9._:-]`, única nas entradas anunciadas da UPF. Este limite é política local, não o máximo normativo do IE 177. Persistir a identidade no YAML e emitir os mesmos bytes em cada arranque; nunca derivar de DNN, posição, endereço N4, relógio ou hash aleatório. Uma alteração explícita é uma substituição válida. Sem histórico persistente não é possível detetar uma alteração acidental entre reinícios; não rejeitar «instabilidade» indetetável. Testar identidade igual com o mesmo YAML, reordenação e mudança N4, e substituição com edição explícita (T24).
- Exemplo de schema pretendido, sujeito à auditoria do parser do HEAD:

  ```yaml
  upf:
    pfcp:
      advertise_ue_ip_pools: true  # default false
    session:
      - subnet: 10.48.0.0/16
        gateway: 10.48.0.1
        dnn: edge1
        pool_id: edge1-v4-a
  ```

- Separar a opção `upf.pfcp.advertise_ue_ip_pools` (default false) de `smf.pfcp.learn_upf_dnn` (default false). A primeira controla emissão; a segunda controla utilização do mapping aprendido. Nenhuma ativa seleção estrita. Não aplicar automaticamente às NFs EPC que partilham a biblioteca.
- No anúncio do MVP, uma pool anunciada deve ter DNN explícito. Uma entrada `session` com DNN vazio não deve ser anunciada como wildcard nem interpretada como suporte universal. Suporte futuro para pools genéricas exige uma semântica separada e testes próprios.
- O anúncio não transporta uma cópia completa do YAML da UPF. O endereço N3 é tratado pelo mecanismo existente de túneis; não acrescentar um endereço GTP-U fictício ao grupo 233.
- Não alterar capacidades UEIP/FTUP para tornar o anúncio aparente ou artificialmente válido. O allocator permanece no CP neste MVP.
- Suportar Setup Request enviado pela UPF e Setup Response enviado pela UPF quando a associação é iniciada pela SMF.
- Guardar metadados aprendidos separados dos estáticos, em estruturas owned que preservem Pool Identity, DNN, família IP, S-NSSAI opcional, origem e geração da associação. Não reutilizar `node->dnn[]` para a representação completa nem guardar ponteiros para buffers da mensagem.
- Preservar o agrupamento pool/DNN/família IP. Se forem recebidas restrições de S-NSSAI, não transformar esses DNNs em capacidades universais: excluir no MVP grupos com qualquer S-NSSAI do braço de match DNN, com diagnóstico claro. Famílias explícitas são avaliadas face à sessão; família ausente é desconhecida, não prova de suporte. Localização e fallback legacy podem ainda escolher o peer. O MVP não promete descoberta/seleção completa por slice.
- Suportar até 16 IE 233 por mensagem, 16 identidades e 8 S-NSSAIs por grupo; Network Instance/IP Version são singletons. Excesso do limite local produz estado de capacidades unsupported e diagnóstico, sem publicar uma lista truncada nem rejeitar automaticamente a associação. Erros de comprimento têm tratamento próprio. Um único campo gerado não satisfaz este requisito. A UPF emite um grupo/identidade por subnet explícita.
- Usar o formato R19: S-NSSAI (257) tem quatro octetos de valor; IP Version (258) exige pelo menos V4 ou V6, permitindo ambos. Zero família com framing válido é erro semântico opcional. Emitir spare bits a zero e ignorá-los na receção; preservar campos conhecidos ao ignorar IEs opcionais/CO desconhecidos. Features válidas mais longas que a struct local não justificam rejeição automática; cópia limitada e armazenamento limpo, sem bytes de associações anteriores.
- Distinguir anúncio ausente, grupo recebido sem DNN utilizável e anúncio inválido. A ausência de todos os IE 233 não transporta um marcador separado de «conjunto válido vazio» e é indistinguível, no wire, de um peer legacy que não implementa o anúncio. Não inventar um significado protocolar que não pode ser observado. Nenhum destes estados significa automaticamente «serve todos os DNNs».

### 3.2 Mapping runtime e seleção legacy (MVP obrigatório)

- O resultado funcional do MVP é equivalente a preencher em runtime os DNNs atualmente fornecidos por `client.upf[].dnn` para um peer que se associou dinamicamente. A associação identifica o peer; o anúncio fornece os metadados DNN.
- O seletor existente tem de consultar os DNNs aprendidos, diretamente ou através de uma vista efetiva static/learned. Apenas armazenar os dados sem os ligar a `compare_ue_info()` ou ao helper que o substituir não conclui o MVP.
- Preservar no MVP a política legacy: DNN OU localização e fallback round-robin associado quando permitido. O HEAD tem ainda um último recurso que devolve o primeiro peer, mesmo sem associação ou com `no_pfcp_rr_select`. Isto pode escolher uma UPF incompatível ou indisponível. Não prometer exclusão absoluta nem sucesso após retries. Os testes principais exigem candidata associada e ausência de localização concorrente.
- Com aprendizagem ativa, os grupos utilizáveis do novo snapshot learned substituem static. Apenas ausência de todos os IE 233 restaura static. Presença de grupos inválidos, unsupported ou sem DNN utilizável não recupera silenciosamente static; grupos independentes válidos podem sobreviver num snapshot degraded, publicado atomicamente. Sem aprendizagem ativa usa-se static. Não fazer união; diagnosticar conflitos. Estes estados locais não são novos estados no wire.
- Um peer dinamicamente associado sem anúncio e sem mapping estático continua sem metadados DNN; o comportamento legacy pode ainda selecioná-lo pelo fallback. Isto é uma limitação conhecida, não uma propriedade a esconder nos testes.
- Múltiplas candidatas com o mesmo DNN continuam a usar a rotação atual. O MVP não acrescenta seleção por carga/latência, fairness global ou nova política de TAC/Cell ID.
- Não mexer em sessões estabelecidas apenas porque uma capacidade foi adicionada ou substituída. Falha/reinício de uma UPF pode afetar as suas sessões segundo o comportamento existente; não prometer continuidade nesse caso.

### 3.3 Seleção estrita (hardening opcional)

- Esta fase é independente do critério de conclusão do MVP e só deve ser implementada depois de provar o mapping runtime com a política legacy.
- Disponibilizar um modo opcional, com nome escolhido conforme o estilo do projeto. Sem o ativar, preservar integralmente o comportamento de seleção anterior.
- No modo estrito, associação PFCP válida e compatibilidade DNN são pré-condições. TAC/Cell ID não ultrapassam uma incompatibilidade DNN.
- Lista estática explícita e anúncio válido explícito: usar a interseção como proposta inicial. Anúncio sozinho: usar os DNNs aprendidos. Anúncio totalmente ausente permite fallback para uma lista estática explícita por compatibilidade legacy; sem lista estática, excluir o peer da seleção estrita.
- Um anúncio válido é autoritativo: omitir um DNN significa não o aceitar nesse peer. Anúncio inválido impede elegibilidade dinâmica e não conserva silenciosamente o snapshot anterior.
- A API interna de seleção deve poder devolver explicitamente «sem candidata». Não selecionar o primeiro peer e não abortar com assert. Como a atribuição de IP ocorre antes da seleção no caminho observado, provar a libertação do IP, contexto SM e restantes recursos, bem como a resposta ao UE.
- Escolha por localização e rotação só pode percorrer candidatas elegíveis. Múltiplas candidatas equivalentes podem usar a rotação existente limitada a esse conjunto.

### 3.4 Ciclo de vida

- Separar resultado da associação de resultado das capacidades. Validar requisitos da associação antes de enviar Request Accepted; verificar Cause e requisitos aplicáveis nas respostas, bem como resultado de envio/commit. Só sucesso da associação permite transição inicial/publicação. Erros semânticos opcionais são ignorados para o protocolo e não impedem automaticamente a associação; excluir conteúdo inutilizável, preparar todos os grupos aceites e publicar um snapshot consistente de uma só vez. Erros estruturais de comprimento seguem §7.6.3/§7.6.7. A ausência de um filho obrigatório dentro do grupo opcional não o transforma num requisito obrigatório de toda a mensagem.
- Nas respostas erradas correlacionadas, parar retransmissões e notificar a aplicação também quando parsing/Node ID falham antes do handler. Não completar uma transação diferente nem inferir correlação de um header demasiado curto. Nas condições R19, verificar apenas o que é verificável; não fabricar UPF Instance ID a partir de pool_id, NRF ou flags de capacidades novas. Reconfirmar na etapa 3 se há identidade NF disponível nos paths UPF.
- Reassociação válida substitui o snapshot, sem duplicações. Testar retirada de DNNs, mudança de identidade e ausência de anúncio na nova associação. Uma nova associação sem IE 233 invalida o snapshot aprendido da geração anterior; pode depois aplicar-se apenas o fallback estático definido em 3.2.
- Verificar os resultados dos handlers também em `associated`. Reassociação aceite substitui o snapshot; tentativa rejeitada não sobrescreve uma associação anterior ainda válida. Resposta tardia/sem transação não repõe dados. Timeout ou reinício detetado invalida learned, incluindo paths de recuperação sem saída do estado associado; libertar no destrutor e preservar static. As buscas normais excluem peers indisponíveis, mas o último fallback legacy pode devolvê-los; corrigir esse comportamento fica fora de 4A.
- Não adicionar Session Retention ou alterar Recovery Timestamp para obter artificialmente continuidade. Preservar a semântica de recuperação existente.
- A conformidade do baseline com retenção/eliminação de sessões em Setup aceite para Node ID já existente (§6.2.6.2.2) não foi demonstrada. T11/T15 devem separar substituição de metadados, adição de peer novo e reassociação do mesmo peer; não alegar continuidade por atualizar o snapshot.
- Association Update sem reassociação fica fora da primeira entrega. Assim, adicionar uma UPF nova funciona através de Setup, mas alterar pools/DNNs de uma UPF já associada só fica visível depois de reassociação/reinício controlado. Documentar esta limitação em vez de alegar atualização arbitrária da configuração em runtime.

### 3.5 Endereçamento do MVP

Usar pools explícitas e controladas pela SMF. UPFs candidatas ao mesmo DNN devem conseguir servir os endereços que a SMF atribui. Para a primeira experiência, pode usar-se a mesma pool por DNN nas candidatas apenas com um único allocator SMF e com N6/retorno comprovadamente corretos. Isto não autoriza múltiplos allocators independentes a distribuir endereços da mesma pool.

Antes de aceitar o resultado do teste, provar os seguintes invariantes:

- nenhum endereço UE é atribuído simultaneamente a sessões diferentes;
- o tráfego de retorno chega à UPF que detém a sessão, incluindo quando duas UPFs anunciam o mesmo DNN/prefixo;
- a topologia usa rotas, NAT, partição de pools ou outro mecanismo N6 explícito; a aprendizagem PFCP não cria esse mecanismo;
- se no futuro existirem várias SMFs allocators, as pools não se sobrepõem sem coordenação externa.

DNNs diferentes podem usar pools diferentes sem mudar a ordem IP -> seleção: A/internet/10.45.0.0/16 e B/edge1/10.48.0.0/16 são um caso do MVP. A limitação aplica-se a várias candidatas para o MESMO DNN/família com pools incompatíveis com os IPs que a SMF pode atribuir; aí seria necessário coordenar escolha de UPF/pool e allocator, fora do âmbito. O IE 233 não transporta prefixos/ranges, por isso pool_id igual ou diferente não permite inferir compatibilidade. As precondições são verificadas na configuração/topologia de teste, não por rejeição automática PFCP. A ordem observada é do caminho normal 5GC; os callers EPC atuais selecionam antes de alocar.

O anúncio de um DNN desconhecido não cria uma pool, um serviço NRF ou uma autorização de subscriber. A alteração permite novas UPFs para serviços existentes.

## 4. Matriz de rastreabilidade inicial

O Codex deve preencher os resultados com evidência, mantendo esta distinção entre protocolo e política local.

| Referência | Natureza | Verificação a implementar |
|---|---|---|
| S1 §6.3.3.1–3 | Arquitetura/política | Seleção na SMF; learned alimenta apenas o braço DNN do legacy |
| S2 §6.2.6.3; §7.4.4.1 | Procedimento | UPF iniciadora; só publicar learned após associação aceite; fallback legacy não é filtragem estrita |
| S2 §7.4.4.2 | Mensagem | Associação iniciada pela SMF também aprende anúncio |
| S2 tabela 7.4.4.1-3 | Estrutura | Fixture independente e descodificador confirmam grupo e filhos |
| S2 §8.2.128; §8.2.4 | Codificação | Pool Identity e Network Instance têm bytes/comprimentos corretos |
| S2 §5.21.1–2 | Alocação | SMF mantém allocator; flags anunciadas continuam verdadeiras |
| S2 tabela 7.4.4.1-3 | Multiplicidade | Vários IE 233 são preservados com limites e sem truncagem silenciosa |
| Política MVP | Mapping runtime | Learned DNN alimenta o seletor existente como o mapping YAML atual |
| Política MVP | Compatibilidade | Seleção e fallback legacy permanecem inalterados |
| Hardening opcional | Elegibilidade estrita | Sem candidata, erro controlado; TAC não contorna DNN |
| S2 §7.2.3.2; §7.6 | Protocolo e política separados | Rejeição da associação não publica; erro semântico opcional não rejeita automaticamente; reassociação tem resultado explícito |
| S2 §7.6.1/3/6 | Respostas erradas | Parar retransmissões e notificar aplicação para resposta correlacionada, mesmo descartada antes do handler |
| S2 §7.4.4.1–2; §7.6.1/8/9 | Condições/defaults e compatibilidade R19 | Condições verificáveis; opcionais/CO desconhecidos e spare bits; precedência só para 233 |
| Política MVP | Ciclo de vida | Invalidar learned em timeout/restart; documentar exceção do último fallback legacy |
| Política proposta | Endereçamento | Pool ID explícito; unicidade de UE IP e retorno N6 comprovados |

A página PDF completa não deve ser copiada para a documentação. Usar links, versões, cláusulas e resumos curtos. Não redistribuir os documentos 3GPP no PR sem verificar a licença.

## 5. Testes de aceitação

O MVP obrigatório cobre T01–T08 e T11–T27, com T19/T20 avaliados segundo a semântica legacy de 3.2. T09 e T10, bem como as variantes estritas de conflito/fallback, pertencem à hardening opcional de 3.3. Não bloquear a entrega do mapping runtime por T09/T10 se a fase opcional não tiver sido autorizada; nesse caso marcá-los `NOT RUN (optional)`.

| ID | Caso | Evidência/resultado esperado |
|---|---|---|
| T01 | Baseline sem alteração | Build e testes existentes registados; reproduzir resultado original quando possível |
| T02 | Codec de um grupo | Bytes independentes corretos; encode/decode; decoder externo |
| T03 | Múltiplos grupos/DNNs | Todos os grupos presentes, sem truncagem ou perda silenciosa |
| T04 | Input inválido/limites/repetições | Separar comprimentos errados, excesso local, identidade ausente/vazia/longa e conflito de identidade; duplicados idênticos deduplicam e singletons usam primeiro valor; sem crash/overread/publicação parcial |
| T05 | Setup iniciado pela UPF | SMF aprende DNN sem peer/DNN estático; captura do anúncio |
| T06 | Setup iniciado pela SMF | Mesmo resultado a partir da resposta da UPF |
| T07 | A: internet; B: edge1, pools distintas | Com peers associados e sem match concorrente de localização, primeiro pedido vai para a candidata correta |
| T08 | B e C: edge1, pools compatíveis | Sem localização concorrente, ambas entram na rotação controlada; A não recebe edge1 neste cenário |
| T09 | Hardening: DNN incompatível, TAC coincidente | TAC não torna essa UPF elegível no modo estrito |
| T10 | Hardening: nenhuma candidata | Sem Session Establishment numa UPF incompatível; falha controlada; IP e contexto previamente criados são libertados |
| T11 | Reassociação | Snapshot substituído; ausência de anúncio remove learned antigo; sem duplicados/dangling pointers |
| T12 | Heartbeat/falha de peer | Com alternativa associada, buscas saltam o peer indisponível; cenário sem alternativas regista último fallback, sem garantia estrita |
| T13 | Interoperabilidade legacy | Feature off, peer sem anúncio, mapping estático e conflito de fontes |
| T14 | Anúncio de DNN não provisionado | Não cria DNN/pool/NRF/subscriber automaticamente |
| T15 | Sessão ativa em A; adicionar B | Sessão de A não é migrada ou apagada pela simples adição de B |
| T16 | IPv4/IPv6 e restrições de slice | Não produzir match DNN por perda de contexto; localização/fallback continuam possíveis |
| T17 | Regressões partilhadas | Caminhos SMF/SGW-C/SGW-U afetados por lib/pfcp compilam e testes relevantes passam |
| T18 | Associação/handler falha | Sem transição inicial/publicação nova; verificar send failure e Cause. Reassociação rejeitada preserva associação anterior só se ainda válida |
| T19 | Ausente versus inválido | Ausente limpa learned e aplica static; semântica opcional inválida não rejeita associação e exclui grupos afetados; não conservar snapshot antigo nem inventar wildcard |
| T20 | Pool com DNN vazio | MVP não a anuncia como wildcard e produz diagnóstico claro |
| T21 | Mesmo DNN, várias pools | Pool identities permanecem distintas; família e contexto não são aplanados |
| T22 | Mesmo DNN em várias UPFs | Pools compatíveis: UE IP único/retorno correto; pools incompatíveis: caso negativo documentado, sem alegar deteção pelo IE 233 |
| T23 | Mudança da UPF em runtime | Sem Association Update, mudança não é alegada como visível antes de nova associação |
| T24 | Estabilidade de pool_id | Comparar bytes após reinícios reais, reordenação e mudança N4; edição explícita substitui identidade, sem rejeição por instabilidade indetetável |
| T25 | Classes de erro PFCP | Request/response, estado inicial/reassociação; obrigatório vs opcional, semântica vs framing, Cause ausente/rejeitada e Node ID em erro. Para respostas erradas correlacionadas, provar fim das retransmissões e notificação; contrastar resposta sem transação/header não identificável |
| T26 | Limitações legacy | DNN incompatível/TAC coincidente, sem match, RR desativado e todos indisponíveis; caracterizar resultado real, sem impor política estrita |
| T27 | Perfil Release 19 | Fixtures R19 request/response; tipos/repetições preservados, desconhecidos opcionais/CO, slice quatro bytes, V4/V6/both/zero, spare bits, features longas e condições UPF Instance ID/UN6DM. Não aplicar precedência de 233 a outros opcionais; detalhado na revisão R19 |

Para T07/T08, não configurar TAC/Cell ID concorrentes e assegurar peers compatíveis associados e pools servíveis. Testar separadamente o contraexemplo de localização em T26. Executar tentativas sequenciais controladas e distinguir cada pedido inicial de retries. Não usar «acabou por fazer ping» como prova. Registar SUPI/IMSI, PDU Session ID, DNN, slice, endereço do UE, Node ID/N4, SEIDs e TEIDs quando disponíveis, timestamps e tentativa. Correlacionar logs SMF com PFCP e GTP-U. Confirmar ausência de reinício da SMF e mudanças de configuração durante a experiência.

As repetições provam ausência de encaminhamento incorreto nos casos testados, não uma garantia estatística de desempenho ou justiça global de distribuição. O tamanho da campanha deve ser documentado.

## 6. Como usar os prompts

Guardar este documento no fork como `OPEN5GS_DYNAMIC_DNN_PLAN.md`. Executar os prompts por ordem na extensão Codex, saltando 4B salvo autorização explícita para a hardening opcional. Uma etapa está concluída quando o seu critério de saída foi verificado, ou quando a limitação está explicitamente registada. O envio do prompt seguinte autoriza essa etapa; o prompt de auditoria não autoriza alterações funcionais.

Os nomes de ficheiros de documentação abaixo são sugestões. Adaptar à organização do repositório e manter um índice. Documentação reutilizável em inglês; explicações ao utilizador podem ser em português. A entrega final não inclui PR upstream.

Não é obrigatório ter acesso ao cluster para concluir auditoria, codec, lógica e testes locais. Se faltarem privilégios ou ferramentas, registar o impedimento e continuar o trabalho independente; não apresentar comandos preparados como testes executados.

### Prompt 1 — Auditar o fork, a norma e fechar o desenho

```text
Lê OPEN5GS_DYNAMIC_DNN_PLAN.md e executa apenas a etapa 1. Nesta etapa não alteres o comportamento do core.

Objetivo: descoberta dinâmica dos DNNs das UPFs por PFCP, para DNNs/pools já provisionados na SMF, com seleção estrita opcional. Não implementar SUPI/API, orchestrator, hot reload de DNNs, Association Update completo, espera por deployments ou migração.

1. Lê AGENTS.md e as instruções aplicáveis. Regista git status, branch, remotes sem credenciais, HEAD, versão/tag e alterações locais. Preserva trabalho existente. Identifica o commit de base; não troques automaticamente para v2.8.0. Procura implementações/PRs equivalentes se houver acesso upstream, sem publicar nada.
2. Mapeia o percurso YAML UPF -> pools -> Setup Request/Response -> parsing -> peer PFCP -> seleção SMF -> criação da sessão. Confirma no HEAD os ficheiros/funções apontados no plano e os callers. Identifica todos os utilizadores relevantes de lib/pfcp, incluindo EPC.
3. Lê as fontes oficiais S1/S2 Release 19 V19.5.0 indicadas e a revisão R19. Constrói uma matriz requisito/cláusula/natureza obrigatória-condicional-opcional-política/decisão/teste. Verifica a aplicabilidade do IE 233 a pools configuradas com allocator no CP; não presumas que basta colocar uma string DNN no pacote. Audita flags de funcionalidades e codificação do Pool Identity. Verifica §7.2.3.2/§7.6, condições dos outros IEs e fim das retransmissões/notificação nas respostas erradas correlacionadas. Se houver contradição normativa, apresenta-a concretamente e propõe alternativa sem implementar um protocolo proprietário.
4. Audita message.h/message.c, os descritores, o gerador pfcp-tlv.py e caches/fontes de geração: tipo 233 vs 93, filho 177, repetições, limites, ordem e ownership dos buffers. Mostra trechos pequenos e links com commit. Não concluas suporte apenas por existir uma struct.
5. Fecha um desenho executável para o MVP: confirma o nome final do novo campo equivalente a `pool_id`, respetivos limites/unicidade/estabilidade, comportamento de pools com DNN vazio, ativação opcional, precedência static/learned, distinção observável entre anúncio ausente e inválido, limitação de não existir «vazio explícito», famílias IP, restrições de slice, snapshots de reassociação, malformed input e cleanup. Regista separadamente o desenho opcional de seleção estrita/erro sem candidata, sem o tornar gate do MVP. Defende decisões com código ou marca-as como política proposta. Não uses nomes de opções inventados como se existissem.
6. Inspeciona Meson, testes e documentação de build do HEAD. Prepara um ambiente/prefixo isolado para build baseline e testes locais viáveis. Não instales sobre serviços do sistema nem alteres o cluster. Regista baseline, dependências e impedimentos reais.

Cria documentação de análise e um ADR com o contrato e o mapa de alterações, em inglês, e um estado de trabalho. Fecha com scope, diferenças face ao plano, questões técnicas não resolvidas e critérios da próxima etapa. Para no fim desta auditoria; não comeces a implementação.
```

Critério de saída: commit identificado; fonte normativa e política local separadas; desenho explícito; baseline ou impedimentos registados. Se o mecanismo escolhido não for justificável, rever antes do prompt 2.

### Prompt 2 — Corrigir/provar o codec PFCP

```text
Executa apenas a etapa 2 de OPEN5GS_DYNAMIC_DNN_PLAN.md, seguindo o ADR da auditoria. A prioridade é provar a codificação PFCP antes de usar os metadados na seleção.

1. Implementa as correções mínimas necessárias aos descritores/estruturas dos IEs e mensagens de associação. Trata esta correção do codec como um bug protocolar independente e separável da feature de seleção. Não modifiques ainda a seleção de UPF nem introduzas API/SUPI.
2. Corrige a fonte verdadeira da geração, incluindo caches se forem usados. Regenera de forma reproduzível a partir do DOCX R19.5.0 identificado, confrontado com S2 V19.5.0. Se faltarem fontes, regista o problema e resolve a origem; não entregues apenas uma edição manual que a regeneração apague. Revê o diff para excluir alterações de IEs não relacionados e não perder definições R19 existentes.
3. Valida tipos, nesting, multiplicidade e limites no grupo 233 e nos filhos, incluindo identidades e S-NSSAIs repetidos. Corrige também `tlv_more`/`max_tlv_more` ou o mecanismo equivalente para representar vários grupos, com capacidade limitada e resultado explícito em excesso. Distingue limitação local/erro semântico opcional de framing inválido; não uses um erro genérico que obrigue o handler a rejeitar qualquer associação. Implementa o encoding do Pool Identity com todos os campos de comprimento exigidos, usando helpers existentes quando adequados. Trata Network Instance conforme o formato definido no ADR, sem assumir string C terminada em NUL recebida da rede. Repetições de singletons usam o primeiro valor segundo §7.6.10.
4. Adiciona testes no framework existente, ajustando dependências Meson quando necessário: request e response, múltiplos grupos, múltiplas identidades onde permitido, ausência de opcionais, duplicados, limites e bytes malformados. Inclui a parte codec de T27 R19 (desconhecidos opcionais, slice, IP Version, spare bits e features longas). Inclui fixture construída/verificada independentemente do encoder em teste; round-trip sozinho não é suficiente.
5. Gera uma captura/fixture e descodifica com uma implementação independente disponível, registando versão, comando e resultado. Não inventes nomes de campos tshark: consulta os campos disponíveis. Se não houver decoder, mantém o teste de bytes e marca a verificação externa como pendente.
6. Compila e executa os testes relevantes. Regista exatamente os comandos e resultados. Atualiza a documentação do formato, diferenças do codec e limitações. Não alteres tráfego/deployments do cluster.

Para no fim. Entrega o diff explicado, evidência binária e regressões verificadas. Não concluas interoperabilidade só porque os dois lados alterados se entendem.
```

Critério de saída: mensagem correta comprovada por fixture independente; testes negativos; correção reproduzível após regeneração. O decoder externo pode ficar pendente se indisponível, mas é necessário antes de alegar validação do protocolo end-to-end.

### Prompt 3 — Anunciar e aprender capacidades

```text
Executa apenas a etapa 3, usando o codec validado e o ADR. Implementa anúncio pela UPF e armazenamento na SMF; mantém a seleção legacy até à etapa seguinte.

1. Constrói os anúncios a partir das pools/DNNs reais da UPF segundo o schema decidido. Implementa e documenta `pool_id`; quando o anúncio estiver ativo, recusa configuração com identidade ausente, vazia, duplicada, fora do formato ou demasiado longa. Usa identidade explícita persistida e testa estabilidade segundo T24, sem rejeitar instabilidade indetetável. Não anuncies pools com DNN vazio como wildcard. Implementa separadamente os controlos de anúncio UPF e aprendizagem SMF do ADR. Não cries DNNs ou allocators na SMF. Não anuncies capacidade de alocação UPF que não esteja implementada.
2. Integra nos builders UP de Setup Request e Setup Response, preservando mensagens normais, recursos e lifetime dos buffers. Não serializes ponteiros, estruturas C brutas ou strings fora do comprimento real.
3. No CP, valida e copia o anúncio para dados owned associados ao peer. Distingue learned de static. Mantém o contexto de grupo/família/slice; não aplanes uma restrição em autorização universal. Não retenhas referências ao buffer da mensagem ou ao YAML sem um lifetime definido.
4. Publica atualizações atomicamente no modelo de eventos do projeto. Trata múltiplos DNNs, duplicados e limite de capacidade. Reassociações idênticas são idempotentes; uma nova associação válida não acumula capabilities antigas. Implementa literalmente a distinção entre anúncio ausente e inválido e não inventes um estado wire de «vazio explícito». Input inválido não publica um snapshot parcial. Distingue erros semânticos opcionais, capacidade não suportada e falha real da associação conforme o ADR. Verifica Cause, send/commit e resultado dos handlers antes da transição inicial, também nos paths de reassociação. Só falha da associação impede essa transição; capacidades opcionais inutilizáveis podem coexistir com associação aceite. Não sobrescrevas uma associação anterior ainda válida por uma tentativa rejeitada. Cumpre T25 R19 também para respostas descartadas antes do handler: correlação segura, fim de retransmissões e notificação. Aplica as condições/defaults específicos dos outros IEs conforme T27 e reconfirma disponibilidade de UPF Instance ID, sem a fabricar.
5. Integra invalidação/cleanup com os caminhos reais de indisponibilidade, reinício/reassociação e destruição do peer. Mantém dados estáticos e a lógica existente de recuperação de sessões. Não prometas atualizar um YAML em live sem um mecanismo que o leia; Association Update completo fica fora deste patch.
6. Acrescenta logs de debug úteis: peer/Node ID, origem, DNNs aprendidos e invalidade, sem excesso no hot path. Adiciona testes de request/response, substituição, ausência, malformed input e lifetime. Usa sanitizers quando suportados e relevantes.

Atualiza design, configuração e testes executados. Para no fim: comprovar anúncio e aprendizagem sem afirmar que já há seleção dinâmica estrita.
```

Critério de saída: a informação passa da configuração da UPF ao peer da SMF com lifetime controlado e sem reinício da SMF; ambos os iniciadores de associação estão cobertos.

### Prompt 4A — Usar o mapping aprendido no seletor legacy (MVP obrigatório)

```text
Executa apenas a etapa 4A. Liga os DNNs aprendidos por PFCP ao seletor existente, reproduzindo o efeito de `client.upf[].dnn` sem alterar ainda a política legacy de seleção.

1. Faz o seletor consultar uma vista efetiva dos metadados static/learned do peer. A projeção utilizável do snapshot fornece os DNNs runtime; apenas ausência de anúncio preserva o mapping estático quando a aprendizagem está ativa. Anúncio inválido/unsupported não recupera static automaticamente. Feature off mantém static. Implementa a precedência decidida no ADR e diagnostica conflitos sem união silenciosa.
2. Preserva a política de `compare_ue_info()` e os fallbacks: DNN OU localização podem produzir match; o braço learned avalia família/contexto segundo o ADR. Sem match pode haver RR associado e, por fim, escolha do primeiro peer mesmo indisponível. Testa estas limitações sem as corrigir nesta etapa. Não transformes esta etapa em strict selection.
3. Garante que uma UPF que inicia PFCP e anuncia `edge1`, sem entrada `client.upf` na SMF, fica utilizável pelo mesmo caminho que uma UPF configurada estaticamente para `edge1`. Múltiplas UPFs que anunciam o mesmo DNN mantêm a rotação existente.
4. Preserva o allocator, a ordem IP->seleção, os callers e as sessões estabelecidas. Não introduzas resultado «sem candidata», alterações de erro NAS/PFCP ou cleanup adicional nesta fase.
5. Adiciona logs com origem `static`/`learned`, DNN pedido e peer escolhido. Testa DNNs separados, múltiplas candidatas iguais, feature/anúncio ausente com mapping estático, conflito de fontes, reassociação e perda/retorno do peer.
6. Demonstra o comportamento legacy conhecido quando não há match: documenta a possibilidade de fallback incompatível, mas não a corrijas nesta etapa.

Para no fim. Não executes a etapa 4B sem pedido explícito.
```

Critério de saída do MVP: sem mappings DNN estáticos por peer e sem match concorrente de localização, a primeira nova sessão para um DNN com UPF compatível associada seleciona essa UPF usando o mapping aprendido; a SMF não reinicia e a política de fallback legacy permanece inalterada e documentada.

### Prompt 4B — Seleção estrita e erro sem candidata (opcional)

```text
Executa a etapa 4B apenas se o utilizador autorizar explicitamente a hardening depois de a etapa 4A estar concluída. Implementa um modo opcional de seleção DNN estrita; o caminho legacy continua disponível e é o default salvo decisão contrária no ADR.

1. Separa elegibilidade de preferência/distribuição. A elegibilidade exige associação válida e DNN compatível segundo static/learned. TAC ou Cell ID nunca contornam um DNN incompatível. Aplica também as restrições de família/contexto que o modo consegue avaliar.
2. Mantém round-robin apenas entre candidatas elegíveis. Trata listas vazias, um peer, remoção/reassociação e alterações da lista sem ciclos infinitos ou ponteiros inválidos.
3. Altera a API interna de seleção para representar explicitamente sucesso ou ausência de candidata. Remove, apenas neste modo, a escolha arbitrária do primeiro peer. Propaga a falha com resposta e cleanup apropriados, incluindo libertação de IP/contexto já criado. Verifica callers 5GC/EPC; não uses assert para indisponibilidade normal.
4. Não alteres o allocator, a ordem IP->seleção ou sessões estabelecidas. Uma UPF anunciar um DNN não o provisiona nem o autoriza na rede.
5. Acrescenta T09/T10 e variantes estritas: TAC coincidente/DNN incompatível, sem candidata, feature off, peer legacy, conflito static/learned, perda/retorno e contextos restritos.
6. Adiciona evidência da razão de rejeição e executa regressões. Demonstra que desativar o modo preserva a baseline da etapa 4A.

Para no fim, sem tornar esta hardening requisito retroativo para aceitar o MVP.
```

Critério de saída opcional: o modo estrito nunca envia uma nova sessão para uma UPF incompatível, falha de forma controlada sem candidata e o modo legacy continua a reproduzir a etapa 4A.

### Prompt 5 — Integração local, imagem e pacote de testes

```text
Executa a etapa 5. Valida a combinação num ambiente isolado e prepara imagens reprodutíveis para o meu cluster. Não alteres deployments existentes.

1. Revê os testes existentes de registo/sessões, respetivas dependências e configuração. Acrescenta o menor teste de integração que exercite realmente a associação e o seletor, evitando um mock que contorne o comportamento que queremos provar.
2. Exercita os testes obrigatórios da matriz T01–T27 conforme os recursos disponíveis, segundo o perfil Release 19 V19.5.0. Marca T09/T10 como `NOT RUN (optional)` se 4B não foi autorizada. Começa por uma SMF, uma slice, DNNs internet/edge1, A para internet e B para edge1. Adiciona C para edge1 em runtime. A SMF não pode ter mappings DNN por peer para os testes dinâmicos. Um teste secundário cobre peers estáticos e associações iniciadas pela SMF.
3. Mantém sessões e tentativas identificadas. Verifica o primeiro pedido PFCP, não apenas conectividade após retries. No MVP, regista o fallback legacy quando não há match. Apenas se 4B existir, demonstra erro controlado e libertação de recursos sem candidata.
4. Se o bug pré-existente de resposta de erro sem Node ID interferir, isola-o, documenta-o e cria um teste mínimo. Não diluas uma correção protocolar independente dentro desta feature. Não alteres o parser para aceitar respostas inválidas apenas para passar o teste.
5. Executa regressões pertinentes de lib/pfcp e SMF/UPF e os caminhos partilhados identificados na auditoria. Usa testes de memória dirigidos a parsing/reassociação/cleanup. Classifica falhas como baseline, introduzidas ou ambiente com evidência.
6. Reutiliza a infraestrutura de container existente se servir; caso contrário, cria uma receita mínima de build para SMF/UPF. Confirma dependências na documentação do commit, fixa bases/dependências quando viável e regista SHA do código, dirty diff se existir, toolchain e imagem. Não uses latest como única identidade. Verifica bibliotecas runtime e arranque dos dois executáveis. Se o daemon container não estiver disponível, prepara a receita mas não declares a imagem construída.
7. Prepara fixtures/YAMLs e scripts parametrizados de teste, com endereços e pools de exemplo claramente identificados. Guarda logs, manifest de versões, pcaps e tabela de sessões num diretório de resultados separado. Não copies chaves reais de subscribers para documentação ou commits.

Entrega relatório com PASS/FAIL/NOT RUN, comandos, artefactos, imagens realmente produzidas e runbook para cluster. Não publiques imagens, não faças push nem abras PR.
```

Critério de saída: testes locais reprodutíveis e uma imagem identificável, ou impedimentos explícitos; runbook pronto para ambiente isolado.

### Prompt 6 — Testar no Kubernetes/K3s de forma isolada

```text
Executa a etapa 6 de OPEN5GS_DYNAMIC_DNN_PLAN.md. O objetivo é validar no meu cluster a feature implementada no fork do core. Os ficheiros de emulação/deployment são fixtures de validação, não a implementação da feature.

1. Lê o runbook e os resultados anteriores. Confirma o contexto Kubernetes de teste e o namespace dedicado que eu indicar. Se ainda não estiverem identificados, prepara os manifests/comandos e pede apenas esses valores antes de os aplicar. Não uses o contexto atual implicitamente para modificar a instalação existente.
2. Inspeciona de forma read-only a rede/Multus/N4/N3/N6, recursos, permissões e ferramentas de captura relevantes. Define endereços/pools sem colisões com as NFs existentes e isolamento da descoberta das NFs de teste. Não reinicies nós, não limpes iptables e não substituas a minha SMF normal.
3. Aplica apenas os recursos novos autorizados de teste. Verifica os imageIDs/digests reais das SMF/UPFs alteradas e a configuração montada, em vez de confiar no tag ou appVersion do chart. As outras NFs precisam de uma versão compatível, registada no relatório.
4. Configura DNNs/pools explicitamente na SMF, `pool_id` distintos nas UPFs e subscriptions/UEs de laboratório. A UPF inicia PFCP para a SMF; a SMF não contém mappings estáticos de DNN por UPF no caso principal. Antes de atribuir falhas de tráfego à seleção, confirma unicidade dos UE IPs, N6/retorno para a UPF proprietária da sessão e o mecanismo de routing/NAT usado.
5. Executa os cenários dinâmicos do MVP: A/internet, adicionar B/edge1, adicionar C/edge1, rotação legacy entre peers com o mesmo DNN, falha e regresso de peer, e adição de B enquanto existe uma sessão em A. Executa «nenhuma candidata» e rotação estrita apenas se 4B tiver sido implementada. Não alegues que reiniciar uma UPF preserva sessões nela.
6. Captura PFCP e, para provar dados, GTP-U/tráfego nas interfaces relevantes. Correlaciona SUPI, PDU Session ID, DNN/slice, UE IP, Node ID/N4, SEIDs e TEIDs. Regista retries separadamente e comprova que a SMF não reiniciou ao aprender B/C. Mede tempo até à elegibilidade sem o confundir com tempo de criação do pod.
7. Executa rollback/cleanup só dos recursos criados pelo teste. Mantém a instalação original intacta. Exporta as evidências para o diretório de resultados indicado e atualiza o relatório de testes com comandos e versões.

Se não houver acesso/NET_ADMIN/captura, declara exatamente o que não foi executado. Não inventes resultados nem instales componentes no cluster para contornar permissões.
```

Critério de saída: associação e primeiro pedido de sessão comprovados no cluster, sem reinício da SMF; resultados e rollback verificáveis. O utilizador pode executar esta etapa depois das anteriores.

### Prompt 7 — Revisão final e documentação do projeto

```text
Executa a etapa 7. Faz uma revisão do diff completo contra o commit de base e fecha a documentação. Não faças push nem publiques PR.

1. Revê o código procurando regressões concretas: campos/descritores gerados inconsistentes, buffers com lifetime curto, leaks/double-free, metadados obsoletos, fallback incompatível, asserts provocáveis por tráfego, grupos de slice aplanados, limites silenciosos e caminhos EPC afetados. Corrige problemas encontrados e repete apenas os testes necessários.
2. Verifica a regeneração do codec e a coerência entre fontes/caches e outputs. Não afirmes reprodutibilidade se não foi executada. Confirma que o path legacy continua coberto.
3. Produz documentação em inglês com: problema e comportamento anterior; versão/commit; desenho e opções novas; exemplo completo UPF/SMF; mapping runtime com seleção legacy; modo estrito apenas se implementado; formato PFCP; precedência static/learned; ciclo de vida; uso com ambos os iniciadores; erros e diagnóstico; precondições de IP/N6; instalação de imagens; testes e resultados; rollback; limitações e trabalho futuro.
4. Atualiza a matriz normativa com links, versões, cláusulas, decisões e testes/evidência. Distingue confirmed in code, specified behavior, implementation policy e not experimentally verified. Não escrevas «3GPP certified» ou conformidade global.
5. Mantém explícito que adicionar peers por associação não é hot reload de YAML, descoberta NRF, criação dinâmica de DNNs ou seleção por SUPI. Documenta as restrições do MVP e as diferenças entre associação, elegibilidade e sessão estabelecida.
6. Separa documentação reutilizável de IPs, nomes e observações específicos do meu laboratório. Mantém pcaps volumosos e dados reais fora de Git; usa fixtures mínimas e manifest de evidências.
7. Produz um mapa interno coerente das alterações: codec/geração, capacidades, seleção/testes/docs e correções protocolares independentes. Explica necessidade, comportamento, testes e limitações. Não prepares PR, não reescrevas o histórico e não faças commit/push ou publicação.

Entrega um checklist final com implementado, testado, pendente, riscos concretos e comandos reproduzíveis. Identifica qualquer gate por cumprir; não marques testes de cluster preparados como executados.
```

## 7. Notas de build e operação

O guia oficial S3 usa Meson/Ninja, prefixo local e testes do projeto. O Codex deve confirmar dependências e comandos contra o commit selecionado. Exemplos de forma, a adaptar ao checkout e à versão instalada das ferramentas:

```sh
meson setup build-dynamic-dnn --prefix="$PWD/install-dynamic-dnn"
ninja -C build-dynamic-dnn
meson test -C build-dynamic-dnn --list
```

Escolher os testes concretos a partir dessa listagem. Os testes de integração podem precisar de base de dados, interfaces TUN, SCTP, endereços e privilégios; não executar toda a suite com sudo sobre o host do cluster sem isolar o ambiente. Instalação local e imagem de teste devem manter separação da instalação existente.

Não é necessário criar uma VM nova por princípio. Build e testes unitários podem decorrer no ambiente de desenvolvimento; integração pode usar container/network namespaces ou o namespace Kubernetes de teste, conforme as capacidades disponíveis. Se o ambiente não permitir um tipo de teste, documentar e mover esse teste para o ambiente adequado.

## 8. Evidência e documentação contínua

Manter um índice com, pelo menos:

- baseline e versão do fork;
- análise do código e ADR;
- matriz normativa;
- configuração e guia de utilização;
- matriz de testes, resultados e limitações;
- runbook local/cluster e rollback;
- revisão final e mapa interno das alterações (sem entrega de PR).

Cada execução deve guardar data, commit/diff, comando, exit status, versões das ferramentas, identidade das imagens e paths de artefactos. Um relatório sem capturas pode comprovar testes unitários; não deve alegar ter observado tráfego PFCP/GTP-U.

O critério central de sucesso do MVP é: lançar uma UPF para um DNN previamente provisionado, sem `client.upf[].dnn` estático para esse peer, aprender o mapping durante uma associação aceite e, no cenário controlado sem localização concorrente e com pools compatíveis, encaminhar o primeiro pedido de uma nova sessão para essa UPF usando o seletor legacy, com evidência e sem reiniciar a SMF. A seleção estrita sem fallback incompatível é uma entrega opcional separada.
