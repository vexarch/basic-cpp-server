async function getProducts() {
	try {
		const response = await fetch('http://127.0.0.1:8080/products');
		const data = await response.json();

		const list = document.getElementById('products');
		data.forEach(prod => {
			const li = document.createElement('li');
			li.textContent = prod;
			list.appendChild(li);
		});
	} catch (error) {
		console.error(error);
	}
}

getProducts();

