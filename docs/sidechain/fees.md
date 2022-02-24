## Introduction

This document describes how fees work for XRP cross chain transactions.

When an asset is sent to a door account, that triggers a cross chain
transaction. The federators must be able to all create the destination chain's
transaction, and they all must create the same transaction - the same destination
account, the same destination amount, the same sequence number, ect.

The federators have a minimum fee of 100 drops. If a fee is not specified on the
triggering transaction, this minimum fee is used. However, a sender may use a
transaction's memo field to explicitly specify a fee. This fee must be greater
than or equal to the minimum fee, and it may only be present if the destination
currency is XRP. It is an error otherwise.

A transaction's memo is an array. The first element of this array specifies the
destination address on the destination chain. The second element (index 1 of a
0-based array) can be used to specify the fee on the destination chain. This
field is a hex encoded big-endian unsigned integer specifying the fee in drops.

No matter how the fee is specified, the fee is deducted from the delivered
amount. Since a transaction can fail and trigger a refund, twice the specified
fee is deducted from the destination amount. For example, if a cross chain
transaction is specified for 1 XRP and the fee is 100 drops, then .999800 XRP
(999800 drops = 1000000 (amt) - 2*100 (fee)) will be delivered. If a refund is
required, the same fee value will be used for the refund transaction.

The motivation for doing this is two fold:

1) If a chain's fee rises about the default fee, this allows cross chain
   transactions to continue.

2) Transaction fees are paid by the door account, but are sent on behalf of
   another account. This allows the fee to be paid by the account triggering the
   transaction rather than by the door account.
