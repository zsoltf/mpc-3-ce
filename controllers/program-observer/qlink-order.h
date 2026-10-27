/* Native 4x4 Q-Link grid traversal, shared by Screen authors and surfaces.
 * Slot identities stay native; position is a presentation coordinate.
 * Pinned native format3 helper: 3.10 0x0162e208, 3.9.1 0x014dc8f8;
 * identical 0x104-byte bodies, SHA256
 * 4a19f282a0e3d90b120a9fd2349bea9793c7e3cb661d9961506dec507a2a88c6. */
#ifndef MPC_QLINK_ORDER_H
#define MPC_QLINK_ORDER_H
static inline unsigned qlink_grid_index(unsigned position,unsigned direction){
 if(position>=16)return 16;
 unsigned row=position%4,column=position/4;
 switch(direction){
 case 1:return row+4*(3-column);
 case 2:return column+4*(3-row);
 case 3:return column+4*row;
 default:return position;
 }
}
static inline unsigned qlink_column_position(unsigned index){return index<16?4*(index%4)+3-index/4:16;}
#endif
