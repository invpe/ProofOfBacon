# ProofOfBacon 🥓
A concept of an election process based on beacon broadcasting and capture

## Overview

The Proof of Beacon concept is a demonstration of a decentralized network coordination mechanism using ESP32 devices. It leverages the capabilities of the ESP32 to send and receive custom beacon frames over Wi-Fi and acknowledge their receipt through UDP communications. This project serves as a proof of concept (POC) for using beacon frames for network node elections, where nodes elect a leader based on the visibility of broadcasted beacon messages.

This project is far from being secure, this is a concept.

## Purpose

The primary purpose of this project is to validate the feasibility of using beacon frames for node discovery and leader election in a local network environment. It aims to demonstrate a simple yet effective way to establish which nodes are active and can communicate within a network, which is a foundational requirement for many distributed systems, particularly in scenarios involving IoT devices.

## Functionality

- Beacon Broadcasting: Nodes periodically broadcast a custom beacon frame that includes a unique identifier and other relevant metadata.
- Beacon Reception: Nodes listen for beacon frames and, upon receiving a frame with the expected parameters, process and acknowledge its receipt.
- Acknowledgment via UDP: Nodes send an acknowledgment over UDP to the beacon's originator, confirming that the beacon was received. This acknowledgment includes the beacon's index, allowing the sender to verify that its broadcast was successful.
- Node Management: The system maintains a list of active nodes based on the beacons sent and received, adding and removing nodes based on their last seen activity.

## Network Setup

- SSID and Password Configuration: Nodes connect to a predefined local Wi-Fi network.
- Node find themselves via simple UDP discovery implementation (broadcast)
- UDP Communications: Nodes use a specific UDP port for sending acknowledgments.
- Promiscuous Mode: Nodes operate in promiscuous mode to capture Wi-Fi frames not specifically addressed to them.


## Use Cases

This project is suitable for environments where node coordination without centralized control is desirable, such as in:

- Home automation systems.
- Ad-hoc mobile networks. 
- Experimental setups for testing distributed algorithms.

## Results

```
14:54:59.028 -> Sending Beacon with index 6
14:54:59.124 -> We have ACK on our beacon from 192.168.1.53 Index 6
14:55:09.463 -> PROOF_OF_BACON from yy:zz:aa:bb:cc:dd index 124
14:55:16.292 -> Sending Beacon with index 7
14:55:16.326 -> We have ACK on our beacon from 192.168.1.53 Index 7
14:55:21.239 -> PROOF_OF_BACON from yy:zz:aa:bb:cc:dd index 125
14:55:32.982 -> PROOF_OF_BACON from yy:zz:aa:bb:cc:dd index 126
```
