/* Exact 3.10 native Duplicate Sequence proof. The fixture still substitutes
 * native command execution below; these bytes prove only the admitted ABI. */
static void sequence_duplicate_native_310_checks(void){
 require(guarded_bytes(0x11e190cu,0x3a8u)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x1ad4a4cu,0x1b8u)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x1ad5284u,0x9d4u)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x1ad2568u,0x538u)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x290f390u,0xbcu)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x29410e4u,0xf0u)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x2ead1b8u,0x52fcu)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x1f9ba7cu,0x178u)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x2949214u,0xd98u)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x15f5fe4u,0x22cu)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x934b18u,0x270u)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x948a20u,0x2d8u)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x2945d14u,0x29cu)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x2943bacu,0x1f8u)!=NULL,"3.10 Duplicate whole native range");
 require(guarded_bytes(0x98cc2cu,0x258u)!=NULL,"3.10 Duplicate whole native range");
 static const uint32_t words[][2]={
  {0x11e1938u,0xe5902038u},
  {0x11e194cu,0xe5903024u},
  {0x11e1960u,0xe5942054u},
  {0x11e1964u,0xe5943004u},
  {0x11e1968u,0xe594b050u},
  {0x11e1990u,0xe284104cu},
  {0x11e1950u,0xe1520003u},
  {0x11e1954u,0xa000041u},
  {0x11e195cu,0xe3000668u},
  {0x11e19e0u,0xe5930b38u},
  {0x1ad4af4u,0xe5848030u},
  {0x1ad4ae0u,0xe5847034u},
  {0x1ad4afcu,0xe5849038u},
  {0x1ad4a90u,0xe3a0103au},
  {0x1ad52b0u,0xe5961034u},
  {0x1ad52d4u,0xe5961038u},
  {0x1ad56f0u,0xe5984250u},
  {0x1ad56fcu,0xe1500004u},
  {0x1ad5774u,0xeaffffddu},
  {0x1ad5718u,0xe5960030u},
  {0x1ad52d8u,0xe1a0b000u},
  {0x1ad52e8u,0xe1a08000u},
  {0x1ad5608u,0xe1a0100bu},
  {0x1ad5610u,0xe1a00008u},
  {0x2949218u,0xe1a04000u},
  {0x2949224u,0xe1a0b001u},
  {0x29492a8u,0xe5db1258u},
  {0x2945df0u,0xe5c45258u},
  {0x1ad571cu,0xe5981250u},
  {0x1ad5710u,0xe5860650u},
  {0x1ad5328u,0xe2864040u},
  {0x1ad5418u,0xe1a00004u},
  {0x1ad5420u,0xe1a01008u},
  {0x1ad280cu,0xe5960030u},
  {0x290f3b8u,0xe591cb00u},
  {0x290f3c4u,0xe5911af8u},
  {0x290f3d0u,0xe5d22258u},
  {0x290f3f8u,0xe5803000u},
  {0x290f3fcu,0xe5c02004u},
  {0x290f3ecu,0xe5c03004u},
  {0x290f43cu,0xe59000e4u},
  {0x290f444u,0x15900250u},
  {0x2941134u,0xe2844001u},
  {0x2eaf188u,0xe594337cu},
  {0x2eaf18cu,0xe593330cu},
  {0x2eaf190u,0xe59303e8u},
  {0x29492a8u,0xe5db1258u},
  {0x29492acu,0xe1a00004u},
  {0x2945d18u,0xe1a05001u},
  {0x2945d24u,0xe5d03258u},
  {0x2945d40u,0xe1530005u},
  {0x2945d54u,0xe3550000u},
  {0x2945d60u,0xe5903250u},
  {0x2945df0u,0xe5c45258u},
  {0x2945de8u,0xe2848030u},
  {0x2945decu,0xe3a03014u},
  {0x2945df4u,0xe3a05001u},
  {0x2945e14u,0xe3a03018u},
  {0x2945e8cu,0xe284006cu},
  {0x2945ea8u,0xe5941250u},
  {0x2943c38u,0xe5991000u},
  {0x2943c40u,0xe12fff33u},
 };
 for(unsigned i=0;i<sizeof words/sizeof words[0];i++){
  const unsigned char *p=guarded_bytes(words[i][0],4);uint32_t v=0;require(p!=NULL,"3.10 Duplicate word guarded");memcpy(&v,p,4);require(v==words[i][1],"3.10 Duplicate native instruction");
 }
 static const uint32_t calls[][3]={
  {0x11e19d0u,0xeb23cc1du,0x1ad4a4cu},
  {0x11e1a10u,0xeb105173u,0x15f5fe4u},
  {0x1ad56f8u,0xeb38e74fu,0x290f43cu},
  {0x1ad5720u,0xeb1318d5u,0x1f9ba7cu},
  {0x1ad5618u,0xeb39cefdu,0x2949214u},
  {0x1ad5428u,0xeb39cf79u,0x2949214u},
  {0x1ad2814u,0xeb132498u,0x1f9ba7cu},
  {0x29492b0u,0xebfff297u,0x2945d14u},
  {0x2945d50u,0xa000037u,0x2945e34u},
  {0x2945d5cu,0x1a000043u,0x2945e70u},
  {0x2945d68u,0x1a00005cu,0x2945ee0u},
  {0x2945dacu,0xeb9b3033u,0x1011e80u},
  {0x2945de4u,0xeb0044ffu,0x29571e8u},
  {0x2945e10u,0xebfff765u,0x2943bacu},
  {0x2945e30u,0xebfff75du,0x2943bacu},
  {0x2945e90u,0xeb811ba2u,0x98cd20u},
  {0x2945ea4u,0xa000007u,0x2945ec8u},
  {0x2945eb0u,0xebffec8bu,0x29410e4u},
  {0x2945ebcu,0xeb9b2fefu,0x1011e80u},
  {0x2945ed8u,0xeb947517u,0xe6333cu},
  {0x2945edcu,0xeaffffc1u,0x2945de8u},
  {0x2945ef4u,0xeaffffaau,0x2945da4u},
 };
 for(unsigned i=0;i<sizeof calls/sizeof calls[0];i++){
  const unsigned char *p=guarded_bytes(calls[i][0],4);uint32_t v=0;require(p!=NULL,"3.10 Duplicate branch guarded");memcpy(&v,p,4);require(v==calls[i][1]&&fixture_branch_target(calls[i][0],v)==calls[i][2],"3.10 Duplicate actual call or conditional target");
 }
 require(SEQ_DUP_BLOCK_TO==0x38&&SEQ_DUP_BLOCK_FROM==0x24&&SEQ_DUP_BLOCK_NAV==0x54&&SEQ_DUP_BLOCK_PROJECT==4&&SEQ_DUP_BLOCK_SPARE==0x50&&SEQ_DUP_BLOCK_NAME==0x4c,"3.10 Confirm six-word ABI");
 require(SEQ_DUP_PROJECT_MANAGER==0xb38&&SEQ_DUP_PROJECT_COUNT==0xb00&&SEQ_DUP_PROJECT_SEQUENCES==0xaf8&&SEQ_DUP_SEQUENCE_SLOT==0x250&&SEQ_DUP_SEQUENCE_USED==0x258&&SEQ_DUP_PROJECT_CURRENT==0xe4,"3.10 Duplicate source fields match native words");
 require(SEQ_DUP_EDITOR_ROOT==0x37c&&SEQ_DUP_ROOT_AUDIO==0x30c&&SEQ_DUP_AUDIO_NAVIGATOR==0x3e8,"3.10 Duplicate native navigator owner chain");
 /* The copy invokes the native used setter: equal state bypasses the store;
  * differing paths retain native naming and both notifications. No direct
  * STRB substitution is used to infer native execution in this component. */
}
